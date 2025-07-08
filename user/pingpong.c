#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int p1[2]; // 父进程 -> 子进程
  int p2[2]; // 子进程 -> 父进程
  char buf[1];
  int pid;
  
  // 创建两个管道
  if(pipe(p1) < 0 || pipe(p2) < 0){
    fprintf(2, "pipe error\n");
    exit(1);
  }
  
  // 创建子进程
  if((pid = fork()) < 0){
    fprintf(2, "fork error\n");
    exit(1);
  }
  
  if(pid == 0){ // 子进程
    close(p1[1]); // 关闭写端
    close(p2[0]); // 关闭读端
    
    // 从p1读取
    if(read(p1[0], buf, 1) != 1){
      fprintf(2, "child: read error\n");
      exit(1);
    }
    
    printf("%d: received ping\n", getpid());
    
    // 写入p2
    if(write(p2[1], buf, 1) != 1){
      fprintf(2, "child: write error\n");
      exit(1);
    }
    
    close(p1[0]);
    close(p2[1]);
    exit(0);
  } else { // 父进程
    close(p1[0]); // 关闭读端
    close(p2[1]); // 关闭写端
    
    // 写入p1
    buf[0] = 'a';
    if(write(p1[1], buf, 1) != 1){
      fprintf(2, "parent: write error\n");
      exit(1);
    }
    
    // 从p2读取
    if(read(p2[0], buf, 1) != 1){
      fprintf(2, "parent: read error\n");
      exit(1);
    }
    
    printf("%d: received pong\n", getpid());
    
    close(p1[1]);
    close(p2[0]);
    wait(0); // 等待子进程结束
    exit(0);
  }
}