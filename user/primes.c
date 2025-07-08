#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int p[2]) {
  int prime;
  int n;
  int newp[2];
  
  // 关闭写端，因为这个进程只从管道读取
  close(p[1]);
  
  // 读取第一个数字，这是一个素数
  if(read(p[0], &prime, sizeof(int)) <= 0) {
    exit(0); // 如果没有数字，退出
  }
  
  // 打印这个素数
  printf("prime %d\n", prime);
  
  // 创建新管道，用于传递给下一个进程
  if(pipe(newp) < 0) {
    fprintf(2, "pipe error\n");
    exit(1);
  }
  
  // 创建子进程
  if(fork() == 0) {
    // 子进程递归调用sieve
    sieve(newp);
  } else {
    // 父进程关闭读端，因为它只向管道写入
    close(newp[0]);
    
    // 读取剩余的数字，过滤掉能被prime整除的数字
    while(read(p[0], &n, sizeof(int)) > 0) {
      if(n % prime != 0) {
        // 将不能被prime整除的数字传递给子进程
        write(newp[1], &n, sizeof(int));
      }
    }
    
    // 关闭所有管道
    close(p[0]);
    close(newp[1]);
    
    // 等待子进程结束
    wait(0);
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  int p[2];
  int i;
  
  // 创建第一个管道
  if(pipe(p) < 0) {
    fprintf(2, "pipe error\n");
    exit(1);
  }
  
  // 创建第一个子进程
  if(fork() == 0) {
    // 子进程调用sieve
    sieve(p);
  } else {
    // 父进程关闭读端
    close(p[0]);
    
    // 向管道写入2到35的数字
    for(i = 2; i <= 35; i++) {
      write(p[1], &i, sizeof(int));
    }
    
    // 关闭写端
    close(p[1]);
    
    // 等待子进程结束
    wait(0);
    exit(0);
  }
}