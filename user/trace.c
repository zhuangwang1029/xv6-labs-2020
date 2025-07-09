#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  // 将掩码参数转换为整数
  int mask = atoi(argv[1]);
  
  // 设置跟踪掩码
  if(trace(mask) < 0){
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  // 准备执行命令的参数
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  nargv[argc-2] = 0;
  
  // 执行命令
  exec(nargv[0], nargv);
  fprintf(2, "%s: exec %s failed\n", argv[0], nargv[0]);
  exit(1);
}