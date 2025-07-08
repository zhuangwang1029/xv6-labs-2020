#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *name);

// 字符串比较函数，用于比较文件名
int match(char *s, char *p) {
  return strcmp(s, p) == 0;
}

void find(char *path, char *name) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 打开目录
  if((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  // 获取目录状态
  if(fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // 确保path是一个目录
  if(st.type != T_DIR) {
    fprintf(2, "find: %s is not a directory\n", path);
    close(fd);
    return;
  }

  // 确保路径长度不会溢出
  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }

  // 将路径复制到缓冲区
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';  // 添加路径分隔符

  // 读取目录中的每个条目
  while(read(fd, &de, sizeof(de)) == sizeof(de)) {
    // 忽略无效的目录项
    if(de.inum == 0)
      continue;
      
    // 忽略 "." 和 ".."
    if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;

    // 将文件名复制到路径缓冲区
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;  // 添加字符串结束符

    // 获取文件状态
    if(stat(buf, &st) < 0) {
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }

    // 检查是否匹配
    if(match(de.name, name)) {
      printf("%s\n", buf);
    }

    // 如果是目录，递归搜索
    if(st.type == T_DIR) {
      find(buf, name);
    }
  }

  close(fd);
}

int main(int argc, char *argv[]) {
  if(argc < 2) {
    fprintf(2, "Usage: find path [name]\n");
    exit(1);
  }
  
  if(argc == 2) {
    // 如果只有一个参数，假设在当前目录查找
    find(argv[1], "");
  } else {
    // 否则使用指定的目录和文件名
    find(argv[1], argv[2]);
  }
  
  exit(0);
}