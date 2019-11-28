

/*================================================================
 *   
 *   
 *   文件名称：os_util.h
 *   创 建 者：肖飞
 *   创建日期：2019年11月28日 星期四 14时58分51秒
 *   修改日期：2019年11月28日 星期四 17时19分50秒
 *   描    述：
 *
 *================================================================*/
#ifndef _OS_UTIL_H
#define _OS_UTIL_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#ifdef __cplusplus
}
#endif
#include <string>

typedef struct
{
  uint8_t byte0;
  uint8_t byte1;
  uint8_t byte2;
  uint8_t byte3;
} uint32_bytes_t;

typedef union
{
  uint32_bytes_t s;
  uint32_t v;
} u_uint32_bytes_t;

typedef struct
{
  uint8_t byte0;
  uint8_t byte1;
} uint16_bytes_t;

typedef union
{
  uint16_bytes_t s;
  uint16_t v;
} u_uint16_bytes_t;

int run_cmd(const char *cmd);
#endif //_OS_UTIL_H
