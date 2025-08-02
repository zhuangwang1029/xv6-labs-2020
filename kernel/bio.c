// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13  // 使用质数减少哈希冲突

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
   // 哈希表桶，每个桶有自己的锁和缓存链表
  struct {
    struct spinlock lock;
    struct buf *head;  // 指向该桶中缓存的第一个buffer
  } bucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // 初始化所有桶的锁
  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache.bucket[i].lock, "bcache.bucket");
    bcache.bucket[i].head = 0;  // 每个桶开始时为空
  }
  // 初始化所有buffer并将它们均匀分配到各个桶中
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->timestamp = ticks;
    b->dev = 0;
    b->blockno = 0;
    b->valid = 0;
    
    // 将buffer均匀分配到各个桶中
    int bucket_id = (b - bcache.buf) % NBUCKET;
    b->next = bcache.bucket[bucket_id].head;
    bcache.bucket[bucket_id].head = b;
  }
}

// 哈希函数：根据设备号和块号计算桶索引
static int
hash(uint dev, uint blockno)
{
  return (dev + blockno) % NBUCKET;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket_id = hash(dev, blockno);

  // 首先在对应的桶中查找
  acquire(&bcache.bucket[bucket_id].lock);
  
  // 检查块是否已经缓存在当前桶中
  for(b = bcache.bucket[bucket_id].head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;  // 更新访问时间
      release(&bcache.bucket[bucket_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 没有找到，需要分配一个新的buffer
  // 首先尝试在当前桶中找一个未使用的buffer
  for(b = bcache.bucket[bucket_id].head; b; b = b->next){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->timestamp = ticks;
      release(&bcache.bucket[bucket_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bucket[bucket_id].lock);

  // 当前桶中没有可用的buffer，需要从其他桶偷取
  // 优化：减少锁争用，每次只锁一个桶
  struct buf *lru_buf = 0;
  int lru_bucket = -1;
  uint oldest_time = -1;

  for(int i = 0; i < NBUCKET; i++){
    if(i == bucket_id) continue;  // 跳过目标桶，已经检查过了
    
    acquire(&bcache.bucket[i].lock);
    for(b = bcache.bucket[i].head; b; b = b->next){
      if(b->refcnt == 0 && b->timestamp < oldest_time){
        oldest_time = b->timestamp;
        lru_buf = b;
        lru_bucket = i;
      }
    }
    
    // 如果在当前桶找到了可用的buffer，立即使用，不继续搜索
    if(lru_buf && lru_bucket == i){
      // 从旧桶的链表中移除
      if(bcache.bucket[lru_bucket].head == lru_buf){
        bcache.bucket[lru_bucket].head = lru_buf->next;
      } else {
        for(struct buf *prev = bcache.bucket[lru_bucket].head; prev; prev = prev->next){
          if(prev->next == lru_buf){
            prev->next = lru_buf->next;
            break;
          }
        }
      }
      release(&bcache.bucket[i].lock);
      
      // 将buffer添加到目标桶中
      acquire(&bcache.bucket[bucket_id].lock);
      lru_buf->dev = dev;
      lru_buf->blockno = blockno;
      lru_buf->valid = 0;
      lru_buf->refcnt = 1;
      lru_buf->timestamp = ticks;
      lru_buf->next = bcache.bucket[bucket_id].head;
      bcache.bucket[bucket_id].head = lru_buf;
      release(&bcache.bucket[bucket_id].lock);
      
      acquiresleep(&lru_buf->lock);
      return lru_buf;
    }
    
    release(&bcache.bucket[i].lock);
  }

  if(lru_buf == 0){
    panic("bget: no buffers");
  }

  // 处理找到的LRU buffer（如果没有在上面的循环中处理）
  acquire(&bcache.bucket[lru_bucket].lock);
  
  // 再次检查buffer是否仍然可用
  if(lru_buf->refcnt != 0){
    release(&bcache.bucket[lru_bucket].lock);
    return bget(dev, blockno);  // 递归重试
  }

  // 从旧桶的链表中移除
  if(bcache.bucket[lru_bucket].head == lru_buf){
    bcache.bucket[lru_bucket].head = lru_buf->next;
  } else {
    for(b = bcache.bucket[lru_bucket].head; b; b = b->next){
      if(b->next == lru_buf){
        b->next = lru_buf->next;
        break;
      }
    }
  }
  release(&bcache.bucket[lru_bucket].lock);

  // 将buffer添加到新桶中
  acquire(&bcache.bucket[bucket_id].lock);
  lru_buf->dev = dev;
  lru_buf->blockno = blockno;
  lru_buf->valid = 0;
  lru_buf->refcnt = 1;
  lru_buf->timestamp = ticks;
  lru_buf->next = bcache.bucket[bucket_id].head;
  bcache.bucket[bucket_id].head = lru_buf;
  release(&bcache.bucket[bucket_id].lock);

  acquiresleep(&lru_buf->lock);
  return lru_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket_id = hash(b->dev, b->blockno);
  acquire(&bcache.bucket[bucket_id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 更新时间戳用于LRU
    b->timestamp = ticks;
  }
  release(&bcache.bucket[bucket_id].lock);
}

void
bpin(struct buf *b) {
  int bucket_id = hash(b->dev, b->blockno);
  acquire(&bcache.bucket[bucket_id].lock);
  b->refcnt++;
  release(&bcache.bucket[bucket_id].lock);
}

void
bunpin(struct buf *b) {
  int bucket_id = hash(b->dev, b->blockno);
  acquire(&bcache.bucket[bucket_id].lock);
  b->refcnt--;
  release(&bcache.bucket[bucket_id].lock);
}
