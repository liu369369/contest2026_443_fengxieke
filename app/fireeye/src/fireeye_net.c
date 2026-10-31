/****************************************************************************
 * FireEye - 联网上报模块（W5500 以太网）
 *
 * 功能：
 *   1) 配置静态 IP（网线直连 PC，无 DHCP）并把网卡拉起来；
 *   2) 起一个上报任务，每 N 秒把最新数据以 HTTP POST（JSON）发到上位机；
 *   3) 网络异常只记日志、不影响本地采样/报警/断电（本地闭环优先）。
 *
 * 上报地址与周期见 fireeye_config.h 里的 FIREYEYE_NET_* 宏。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <nuttx/kthread.h>

#include <netutils/netlib.h>

#include <arch/board/board.h>

#include "include/fireeye_config.h"
#include "include/fireeye_net.h"

#ifdef CONFIG_FIREYEYE_NET

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct
{
  float    current_a;
  float    temp_c;
  float    bus_v;
  int      state;
  bool     alarm;
  uint32_t uptime_s;
} g_net_latest;

static bool g_net_started;
static int  g_net_fail_count;
static int  g_net_ok_count;

#define FIREEYE_NET_CONNECT_TIMEOUT_MS 3000
#define FIREEYE_NET_IO_TIMEOUT_SEC     3

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 配置静态 IP 并启用网卡（网线直连，无 DHCP 服务器） */

/* 设置 IP / 掩码 / 网关 / MAC（ifup 前后都要用到，抽成函数） */

static int fireeye_net_apply_addresses(void)
{
  struct in_addr addr;
  int ret;

  if (inet_pton(AF_INET, FIREYEYE_NET_IPADDR, &addr) != 1)
    {
      return -EINVAL;
    }

  ret = netlib_set_ipv4addr(FIREYEYE_NET_DEVNAME, &addr);
  syslog(LOG_INFO, "FireEye net: set ip -> %d\n", ret);

  if (inet_pton(AF_INET, FIREYEYE_NET_NETMASK, &addr) == 1)
    {
      ret = netlib_set_ipv4netmask(FIREYEYE_NET_DEVNAME, &addr);
      syslog(LOG_INFO, "FireEye net: set netmask -> %d\n", ret);
    }

  if (inet_pton(AF_INET, FIREYEYE_NET_GATEWAY, &addr) == 1)
    {
      ret = netlib_set_dripv4addr(FIREYEYE_NET_DEVNAME, &addr);
      syslog(LOG_INFO, "FireEye net: set gateway -> %d\n", ret);
    }

  {
    static const uint8_t mac[6] = FIREYEYE_NET_MAC;
    netlib_setmacaddr(FIREYEYE_NET_DEVNAME, mac);
  }

  return OK;
}

static int fireeye_net_configure(void)
{
  struct in_addr addr;
  int ret;

  if (inet_pton(AF_INET, FIREYEYE_NET_IPADDR, &addr) != 1)
    {
      syslog(LOG_ERR, "FireEye net: bad IP %s\n", FIREYEYE_NET_IPADDR);
      return -EINVAL;
    }

  ret = netlib_set_ipv4addr(FIREYEYE_NET_DEVNAME, &addr);
  syslog(LOG_INFO, "FireEye net: set ip -> %d\n", ret);
  if (ret < 0)
    {
      return ret;
    }

  if (inet_pton(AF_INET, FIREYEYE_NET_NETMASK, &addr) == 1)
    {
      ret = netlib_set_ipv4netmask(FIREYEYE_NET_DEVNAME, &addr);
      syslog(LOG_INFO, "FireEye net: set netmask -> %d\n", ret);
    }

  if (inet_pton(AF_INET, FIREYEYE_NET_GATEWAY, &addr) == 1)
    {
      ret = netlib_set_dripv4addr(FIREYEYE_NET_DEVNAME, &addr);
      syslog(LOG_INFO, "FireEye net: set gateway -> %d\n", ret);
    }

  /* 固定 MAC，便于上位机侧识别设备 */

  static const uint8_t mac[6] = FIREYEYE_NET_MAC;
  netlib_setmacaddr(FIREYEYE_NET_DEVNAME, mac);

  ret = fireeye_net_apply_addresses();
  if (ret < 0)
    {
      return ret;
    }

  ret = netlib_ifup(FIREYEYE_NET_DEVNAME);
  syslog(LOG_INFO, "FireEye net: ifup -> %d\n", ret);
  if (ret < 0)
    {
      return ret;
    }

  /* 驱动初始化后芯片里的 MAC 仍为全零且开着 MAC 过滤，
   * 会导致完全收不到包；这里补写 MAC 并关闭过滤。 */

  static const uint8_t fixmac[6] = FIREYEYE_NET_MAC;
  gd32_fireeye_w5500_fixup(fixmac);

  /* 接口起来后再刷一次地址与掩码：确保按掩码重建直连路由，
   * 否则可能出现"IP 有了但发不出去"（connect 返回 -101 ENETUNREACH）。 */

  if (inet_pton(AF_INET, FIREYEYE_NET_IPADDR, &addr) == 1)
    {
      netlib_set_ipv4addr(FIREYEYE_NET_DEVNAME, &addr);
    }

  if (inet_pton(AF_INET, FIREYEYE_NET_NETMASK, &addr) == 1)
    {
      netlib_set_ipv4netmask(FIREYEYE_NET_DEVNAME, &addr);
    }

  syslog(LOG_INFO, "FireEye net: %s up, ip=%s, server=%s:%d\n",
         FIREYEYE_NET_DEVNAME, FIREYEYE_NET_IPADDR,
         FIREYEYE_NET_SERVER, FIREYEYE_NET_PORT);

  /* 回读实际生效的参数（排查路由问题用） */

  {
    struct in_addr ip;
    struct in_addr mask;
    struct in_addr gw;

    if (netlib_get_ipv4addr(FIREYEYE_NET_DEVNAME, &ip) == OK &&
        netlib_get_ipv4netmask(FIREYEYE_NET_DEVNAME, &mask) == OK)
      {
        char s_ip[INET_ADDRSTRLEN];
        char s_mask[INET_ADDRSTRLEN];
        char s_gw[INET_ADDRSTRLEN];

        netlib_get_dripv4addr(FIREYEYE_NET_DEVNAME, &gw);
        inet_ntop(AF_INET, &ip, s_ip, sizeof(s_ip));
        inet_ntop(AF_INET, &mask, s_mask, sizeof(s_mask));
        inet_ntop(AF_INET, &gw, s_gw, sizeof(s_gw));
        syslog(LOG_INFO, "FireEye net: readback ip=%s mask=%s gw=%s\n",
               s_ip, s_mask, s_gw);
      }
  }

  return OK;
}

/* 一次 HTTP POST；成功返回 0 */

static int fireeye_net_set_timeouts(int sock)
{
  struct timeval tv;

  tv.tv_sec  = FIREEYE_NET_IO_TIMEOUT_SEC;
  tv.tv_usec = 0;

  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
      return -errno;
    }

  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
    {
      return -errno;
    }

  return OK;
}

static int fireeye_net_connect(int sock,
                               FAR const struct sockaddr_in *server)
{
  struct pollfd pfd;
  socklen_t optlen;
  int flags;
  int soerr;
  int ret;

  flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0)
    {
      return -errno;
    }

  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
      return -errno;
    }

  ret = connect(sock, (FAR const struct sockaddr *)server, sizeof(*server));
  if (ret < 0 && errno != EINPROGRESS)
    {
      ret = -errno;
      goto errout;
    }

  if (ret < 0)
    {
      pfd.fd      = sock;
      pfd.events  = POLLOUT;
      pfd.revents = 0;

      ret = poll(&pfd, 1, FIREEYE_NET_CONNECT_TIMEOUT_MS);
      if (ret == 0)
        {
          ret = -ETIMEDOUT;
          goto errout;
        }

      if (ret < 0)
        {
          ret = -errno;
          goto errout;
        }

      soerr = 0;
      optlen = sizeof(soerr);
      if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &optlen) < 0)
        {
          ret = -errno;
          goto errout;
        }

      if (soerr != 0)
        {
          ret = -soerr;
          goto errout;
        }
    }

  if (fcntl(sock, F_SETFL, flags) < 0)
    {
      ret = -errno;
      goto errout;
    }

  return OK;

errout:
  return ret;
}

static int fireeye_net_post(const char *json)
{
  struct sockaddr_in server;
  char request[512];
  char response[256];
  FAR const char *ptr;
  size_t remaining;
  int sock;
  int len;
  int ret;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      syslog(LOG_WARNING, "FireEye net: socket() errno=%d\n", errno);
      return -errno;
    }

  ret = fireeye_net_set_timeouts(sock);
  if (ret < 0)
    {
      close(sock);
      syslog(LOG_WARNING, "FireEye net: setsockopt errno=%d\n", -ret);
      return ret;
    }

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port   = htons(FIREYEYE_NET_PORT);
  inet_pton(AF_INET, FIREYEYE_NET_SERVER, &server.sin_addr);

  ret = fireeye_net_connect(sock, &server);
  if (ret < 0)
    {
      close(sock);
      syslog(LOG_WARNING, "FireEye net: connect() error=%d\n", -ret);
      return ret;
    }

  len = snprintf(request, sizeof(request),
                 "POST %s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n\r\n%s",
                 FIREYEYE_NET_PATH, FIREYEYE_NET_SERVER, FIREYEYE_NET_PORT,
                 (int)strlen(json), json);

  ptr       = request;
  remaining = (size_t)len;

  while (remaining > 0)
    {
      ret = send(sock, ptr, remaining, 0);
      if (ret < 0)
        {
          ret = -errno;
          close(sock);
          return ret;
        }

      if (ret == 0)
        {
          close(sock);
          return -EIO;
        }

      ptr       += ret;
      remaining -= (size_t)ret;
    }

  /* The response is advisory only.  The receive timeout prevents a broken or
   * half-closed HTTP connection from blocking the telemetry task forever.
   */

  ret = recv(sock, response, sizeof(response) - 1, 0);

  if (ret <= 0)
    {
      ret = ret < 0 ? -errno : -EIO;
      close(sock);
      return ret;
    }

  close(sock);
  return OK;
}

/* 上报任务 */

static int fireeye_net_task(int argc, char *argv[])
{
  char json[256];
  char state[16];
  int ret;

  for (; ; )
    {
      const char *name;

      switch (g_net_latest.state)
        {
          case 1:  name = "WARNING";  break;
          case 2:  name = "ALARM";    break;
          case 3:  name = "SHUTDOWN"; break;
          default: name = "NORMAL";   break;
        }

      strncpy(state, name, sizeof(state) - 1);
      state[sizeof(state) - 1] = '\0';

      snprintf(json, sizeof(json),
               "{\"device\":\"%s\",\"ts\":%lu,\"current_a\":%.2f,"
               "\"temp_c\":%.1f,\"bus_v\":%.2f,"
               "\"state\":\"%s\",\"alarm\":%s,\"uptime_s\":%lu}",
               FIREYEYE_NET_DEVICE_ID, (unsigned long)time(NULL),
               (double)g_net_latest.current_a, (double)g_net_latest.temp_c,
               (double)g_net_latest.bus_v,
               state, g_net_latest.alarm ? "true" : "false",
               (unsigned long)g_net_latest.uptime_s);

      ret = fireeye_net_post(json);
      if (ret < 0)
        {
          g_net_fail_count++;

          /* 连续失败较多时尝试重新初始化接口（驱动异常后会 fence 芯片） */

          if ((g_net_fail_count % 10) == 0)
            {
              syslog(LOG_WARNING, "FireEye net: recovering interface\n");
              netlib_ifdown(FIREYEYE_NET_DEVNAME);
              fireeye_net_apply_addresses();

              if (netlib_ifup(FIREYEYE_NET_DEVNAME) == OK)
                {
                  static const uint8_t rmac[6] = FIREYEYE_NET_MAC;
                  gd32_fireeye_w5500_fixup(rmac);
                }
            }

          /* 限频告警：每 12 次失败（约 1 分钟）提示一次 */

          if (g_net_fail_count % 12 == 1)
            {
              syslog(LOG_WARNING, "FireEye net: post failed (%d), retry %d\n",
                     ret, g_net_fail_count);
            }
        }
      else
        {
          if (g_net_fail_count != 0)
            {
              syslog(LOG_INFO, "FireEye net: reconnect ok after %d failures\n",
                     g_net_fail_count);
            }

          g_net_fail_count = 0;

          /* One low-rate success message makes a stalled task distinguishable
           * from an intentionally quiet console.
           */

          if ((++g_net_ok_count % 12) == 0)
            {
              syslog(LOG_INFO, "FireEye net: post ok (%d)\n", g_net_ok_count);
            }
        }

      sleep(FIREYEYE_NET_PERIOD_S);
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int fireeye_net_init(void)
{
  int ret;

  /* Register the W5500 only after the application has started.  Keeping the
   * driver registration out of board bring-up prevents a missing PHY link
   * from delaying or blocking the NSH console.
   */

  ret = gd32_fireeye_w5500_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye net: W5500 initialize failed: %d\n", ret);
      return ret;
    }

  ret = fireeye_net_configure();
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye net: configure failed: %d\n", ret);
      return ret;
    }

  ret = kthread_create("fireeye_net", 110, 3072, fireeye_net_task, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye net: create task failed: %d\n", ret);
      return ret;
    }

  g_net_started = true;
  return OK;
}

void fireeye_net_update(float current_a, float temp_c, float bus_v, int state,
                        bool alarm, uint32_t uptime_s)
{
  if (!g_net_started)
    {
      return;
    }

  g_net_latest.current_a = current_a;
  g_net_latest.temp_c    = temp_c;
  g_net_latest.bus_v     = bus_v;
  g_net_latest.state     = state;
  g_net_latest.alarm     = alarm;
  g_net_latest.uptime_s  = uptime_s;
}

#endif /* CONFIG_FIREYEYE_NET */
