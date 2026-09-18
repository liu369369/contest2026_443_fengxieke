/****************************************************************************
 * FireEye - Local event history in on-chip FLASH
 *
 * 设计说明（不依赖任何外部器件）：
 *   - 用 NuttX 的 progmem 接口，把芯片内部 Flash 中留出的若干擦除块
 *     当成环形记录区，每条记录 24 字节定长，顺序追加；
 *   - 一个块写满后就擦除下一个块继续写，因此任意时刻至少还有若干块
 *     保存着更早的历史；断电重启后由 fireeye_storage_init() 扫描恢复；
 *   - 记录只在"状态发生变化"和"每 60 秒心跳"时写入，避免 10Hz 采样
 *     把 Flash 写坏。
 *
 * 记录格式（24 字节，小端）：
 *   magic(4) seq(4) uptime_s(4) current_a(4,float) temp_c(4,float)
 *   state(1) alarm(1) check(2, 前 22 字节累加和)
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include "include/fireeye_storage.h"

#if defined(CONFIG_FIREYEYE_STORAGE) && defined(CONFIG_ARCH_HAVE_PROGMEM)

#include <nuttx/progmem.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define FIREEYE_REC_MAGIC      0x45594531u  /* "EYE1" */
#define FIREEYE_REC_SIZE       24

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct fireeye_record_s
{
  uint32_t magic;       /* FIREEYE_REC_MAGIC */
  uint32_t seq;         /* 递增序号，从 1 开始，用于排序 */
  uint32_t uptime_s;    /* 记录时刻（系统运行秒数） */
  float    current_a;   /* 滤波后的电流 */
  float    temp_c;      /* 滤波后的温度 */
  uint8_t  state;       /* 状态机状态 */
  uint8_t  alarm;       /* 1 = 报警态 */
  uint16_t check;       /* 前 22 字节累加和 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static size_t   g_blocks;      /* 可用的擦除块数量 */
static size_t   g_block_size;  /* 单个擦除块字节数 */
static size_t   g_block;       /* 当前写入块号 */
static size_t   g_offset;      /* 当前块内的写入偏移 */
static uint32_t g_next_seq;    /* 下一条记录的序号 */
static int      g_count;       /* 已保存的记录条数 */
static bool     g_ready;       /* 是否初始化成功 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 以字节累加和作为校验，能挡住全 0xFF 的空白 Flash 与写了一半的残记录 */

static uint16_t fireeye_rec_check(const struct fireeye_record_s *rec)
{
  const uint8_t *raw = (const uint8_t *)rec;
  uint16_t sum = 0;
  int i;

  for (i = 0; i < (int)offsetof(struct fireeye_record_s, check); i++)
    {
      sum = (uint16_t)(sum + raw[i]);
    }

  return sum;
}

static bool fireeye_rec_valid(const struct fireeye_record_s *rec)
{
  return rec->magic == FIREEYE_REC_MAGIC &&
         rec->check == fireeye_rec_check(rec);
}

/* 读一块里的第 index 条记录；返回 0 表示读到且有效 */

static int fireeye_rec_read(size_t block, size_t index,
                            struct fireeye_record_s *rec)
{
  size_t addr = up_progmem_getaddress(block) + index * FIREEYE_REC_SIZE;

  /* 片内 Flash 是可直接寻址的，按地址读取即可；
   * 空白区域读出 0xFF，会被下面的校验判为无效记录。
   * 这里不用 up_progmem_read()：该接口的声明依赖
   * CONFIG_ARCH_HAVE_PROGMEM_READ，本芯片配置下未打开。
   */

  memcpy(rec, (const void *)addr, sizeof(*rec));

  return fireeye_rec_valid(rec) ? 0 : -EINVAL;
}

/* 该槽位是否还是空白（24 字节全 0xFF）。用来区分空白与残迹：
 * 残迹（例如调试命令写进去的字节）既不是有效记录，也不能直接覆盖。 */

static bool fireeye_slot_blank(size_t block, size_t index)
{
  const uint8_t *ptr = (const uint8_t *)up_progmem_getaddress(block) +
                       index * FIREEYE_REC_SIZE;
  int i;

  for (i = 0; i < FIREEYE_REC_SIZE; i++)
    {
      if (ptr[i] != 0xffu)
        {
          return false;
        }
    }

  return true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int fireeye_storage_init(void)
{
  struct fireeye_record_s rec;
  size_t block;
  size_t index;
  size_t best_block = 0;
  size_t best_index = 0;
  uint32_t best_seq = 0;
  bool write_set = false;
  int total = 0;

  g_blocks     = up_progmem_neraseblocks();
  g_block_size = up_progmem_erasesize(0);
  g_ready      = false;
  g_block      = 0;
  g_offset     = 0;
  g_next_seq   = 1;

  if (g_blocks < 1 || g_block_size < FIREEYE_REC_SIZE)
    {
      syslog(LOG_ERR, "FireEye storage: no usable progmem region\n");
      return -ENODEV;
    }

  /* 安全护栏：存证区不能压在应用镜像上（本板镜像自 0x08000000 起，
   * 512KiB 以内一律视为危险区域；曾经因为配置宏兜底走错分支，
   * 存证区被放到 0x08040000，正好落在镜像中间）。 */

  if (up_progmem_getaddress(0) < 0x08080000ul)
    {
      syslog(LOG_ERR,
             "FireEye storage: region 0x%08lx overlaps the application "
             "image; refusing to use it\n",
             (unsigned long)up_progmem_getaddress(0));
      return -ENODEV;
    }

  /* 逐块扫描：统计有效记录、找出序号最大的一条作为最新，并把写入位置定在
   * 第一条全 0xFF 的空白槽上。
   *
   * 为什么区分空白槽和无效槽：旧逻辑碰到无效槽就停，把写入指针留在残迹上，
   * 之后每次写入都因回读不一致而 EIO 失败。 */

  for (block = 0; block < g_blocks; block++)
    {
      size_t acked = 0;      /* 非空白槽位数 */
      size_t valid = 0;      /* 其中有效记录条数 */
      uint32_t last_seq = 0;
      bool has_free = false;

      for (index = 0; index + FIREEYE_REC_SIZE <= g_block_size; index++)
        {
          if (fireeye_slot_blank(block, index))
            {
              has_free = true;
              break;
            }

          acked = index + 1;

          if (fireeye_rec_read(block, index, &rec) == 0)
            {
              valid++;
              last_seq = rec.seq;
            }
        }

      total += (int)valid;

      if (valid > 0 && last_seq >= best_seq)
        {
          best_seq   = last_seq;
          best_block = block;
          best_index = acked;
          write_set  = true;
        }
      else if (!write_set && has_free)
        {
          /* 没有有效记录：取第一个还有空白槽的块，从它的第一条空白槽写起
           * （块开头若有调试残迹，就自动跳过，不再卡在残迹上）。 */

          best_block = block;
          best_index = acked;
          write_set  = true;
        }
    }

  g_block    = best_block;
  g_offset   = best_index * FIREEYE_REC_SIZE;
  g_next_seq = best_seq + 1;
  g_count    = total;
  g_ready    = true;

  syslog(LOG_INFO, "FireEye storage: write at block %lu + %lu B\n",
         (unsigned long)g_block, (unsigned long)g_offset);

  syslog(LOG_INFO,
         "FireEye storage: base=0x%08lx block=%lu x %lu B, %d records\n",
         (unsigned long)up_progmem_getaddress(0), (unsigned long)g_blocks,
         (unsigned long)g_block_size, g_count);

  return g_count;
}

int fireeye_storage_append(uint8_t state, bool alarm, float current_a,
                           float temp_c, uint32_t uptime_s)
{
  struct fireeye_record_s rec;
  ssize_t ret;

  if (!g_ready)
    {
      return -ENODEV;
    }

  /* 当前块写满：擦除下一个块，环形推进 */

  if (g_offset + FIREEYE_REC_SIZE > g_block_size)
    {
      size_t next = (g_block + 1) % g_blocks;

      ret = up_progmem_eraseblock(next);
      if (ret < 0)
        {
          syslog(LOG_ERR, "FireEye storage: erase block %lu failed (%d)\n",
                 (unsigned long)next, (int)ret);
          return (int)ret;
        }

      g_block  = next;
      g_offset = 0;
    }

  memset(&rec, 0, sizeof(rec));
  rec.magic     = FIREEYE_REC_MAGIC;
  rec.seq       = g_next_seq;
  rec.uptime_s  = uptime_s;
  rec.current_a = current_a;
  rec.temp_c    = temp_c;
  rec.state     = state;
  rec.alarm     = alarm ? 1 : 0;
  rec.check     = fireeye_rec_check(&rec);

  ret = up_progmem_write(up_progmem_getaddress(g_block) + g_offset,
                         &rec, sizeof(rec));
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye storage: write failed (%d)\n", (int)ret);
      return (int)ret;
    }

  g_offset += FIREEYE_REC_SIZE;
  g_next_seq++;
  g_count++;

  return 0;
}

int fireeye_storage_count(void)
{
  return g_ready ? g_count : 0;
}

int fireeye_storage_show(int max)
{
  struct fireeye_record_s rec;
  const char *name;
  uint32_t seq;
  uint32_t first;
  size_t block;
  size_t index;
  uint32_t newest = 0;
  int printed = 0;

  if (!g_ready)
    {
      syslog(LOG_ERR, "FireEye storage: not initialized\n");
      return -ENODEV;
    }

  if (max <= 0)
    {
      max = 10;
    }

  for (block = 0; block < g_blocks; block++)
    {
      for (index = 0; index + FIREEYE_REC_SIZE <= g_block_size; index++)
        {
          if (fireeye_rec_read(block, index, &rec) < 0)
            {
              break;
            }

          if (rec.seq > newest)
            {
              newest = rec.seq;
            }
        }
    }

  if (newest == 0)
    {
      syslog(LOG_INFO, "FireEye storage: no history yet\n");
      return 0;
    }

  first = (newest > (uint32_t)max) ? (newest - (uint32_t)max + 1) : 1;

  syslog(LOG_INFO, "FireEye storage: history %lu..%lu of %d records\n",
         (unsigned long)first, (unsigned long)newest, g_count);

  /* 直接按序号回放，避免把上千条记录读进内存 */

  for (seq = first; seq <= newest; seq++)
    {
      bool found = false;

      for (block = 0; block < g_blocks; block++)
        {
          for (index = 0; index + FIREEYE_REC_SIZE <= g_block_size; index++)
            {
              if (fireeye_rec_read(block, index, &rec) < 0)
                {
                  break;
                }

              if (rec.seq == seq)
                {
                  name = (rec.state == 0) ? "NORMAL" :
                         (rec.state == 1) ? "WARNING" :
                         (rec.state == 2) ? "ALARM" : "SHUTDOWN";

                  syslog(LOG_INFO,
                         "FireEye history #%lu uptime=%lus state=%s "
                         "I=%.2fA T=%.1fC\n",
                         (unsigned long)rec.seq, (unsigned long)rec.uptime_s,
                         name, (double)rec.current_a, (double)rec.temp_c);
                  printed++;
                  found = true;
                  break;
                }
            }

          if (found)
            {
              break;
            }
        }
    }

  return printed;
}

#endif /* CONFIG_FIREYEYE_STORAGE && CONFIG_ARCH_HAVE_PROGMEM */
