#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LINE 512

int main(int argc, char *argv[]) {
  char buf[MAX_LINE];
  char *xargv[MAXARG];
  int n, m, arg_idx, i, j;
  char *p;
  
  if(argc < 2) {
    fprintf(2, "Usage: xargs command [args...]\n");
    exit(1);
  }
  
  // 复制命令和固定参数
  for(n = 1; n < argc; n++) {
    xargv[n-1] = argv[n];
  }
  
  // 从标准输入读取行
  while(1) {
    // 初始化缓冲区
    memset(buf, 0, MAX_LINE);
    
    // 读取一行
    m = 0;
    while(1) {
      n = read(0, &buf[m], 1);
      if(n <= 0 || buf[m] == '\n') {
        break;
      }
      m++;
      if(m >= MAX_LINE) {
        fprintf(2, "xargs: line too long\n");
        exit(1);
      }
    }
    
    // 如果到达文件末尾，退出
    if(n <= 0 && m == 0) {
      break;
    }
    
    // 去掉换行符
    buf[m] = 0;
    
    // 复制固定参数
    arg_idx = argc - 1;
    
    // 解析行内参数
    p = buf;
    while(*p) {
      // 跳过空格
      while(*p && *p == ' ')
        p++;
      
      if(*p == 0)
        break;
      
      // 记录参数开始位置
      xargv[arg_idx++] = p;
      
      // 找到参数结束位置
      while(*p && *p != ' ')
        p++;
      
      // 如果不是结束，添加字符串结束符
      if(*p)
        *p++ = 0;
      
      // 检查参数数量是否超出限制
      if(arg_idx >= MAXARG - 1) {
        fprintf(2, "xargs: too many arguments\n");
        exit(1);
      }
    }
    
    // 设置参数列表结束
    xargv[arg_idx] = 0;
    
    // 创建子进程执行命令
    if(fork() == 0) {
      exec(xargv[0], xargv);
      fprintf(2, "xargs: exec %s failed\n", xargv[0]);
      exit(1);
    }
    
    // 等待子进程结束
    wait(0);
  }
  
  exit(0);
}