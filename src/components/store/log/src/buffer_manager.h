#ifndef __BUFFER_MANAGER_H__
#define __BUFFER_MANAGER_H__

//#define DISABLE_IO
#include <api/memory_itf.h>
#include <api/block_itf.h>

class Buffer_manager
{
public:
  static constexpr size_t IO_BUFFER_SIZE        = KB(4);
private:
  static constexpr size_t IO_BUFFER_ALIGNMENT   = KB(4);
  static constexpr size_t NUM_IO_BUFFERS        = 256;
  
public:
  Buffer_manager(Component::IBlock_device * block, unsigned queue_id, Header& hdr)
    : _block(block),
      _hdr(hdr),
      _queue_id(queue_id),
      _index_ring(NUM_IO_BUFFERS),
      _shadow_tail(hdr.get_tail())
  {
    assert(block);
    /* create IO buffers */
    _iob_buffer = _block->allocate_io_buffer(IO_BUFFER_SIZE *  NUM_IO_BUFFERS,
                                             IO_BUFFER_ALIGNMENT,
                                             Component::NUMA_NODE_ANY);
    assert(_iob_buffer);

    _iob_vaddr = static_cast<byte*>(_block->virt_addr(_iob_buffer));
    assert(_iob_vaddr);
  
    for(uint64_t i=1;i<=NUM_IO_BUFFERS;i++)
      _index_ring.sp_enqueue(i);

    ready_buffer();
  }

  ~Buffer_manager() {
    flush_buffer();
    _block->check_completion(0);
  }

  static void release_buffer(uint64_t guid, void * arg0, void* arg1)
  {
    assert(arg0);
    Buffer_manager * pThis = reinterpret_cast<Buffer_manager*>(arg0);
    uint64_t buffer_index = reinterpret_cast<uint64_t>(arg1);
    pThis->free_index(buffer_index);
  }

  void ready_buffer()
  {
    _index_ring.mc_dequeue(_current_buffer_index);
    _current_buffer_ptr = static_cast<byte*>(_iob_vaddr +
                                             ((_current_buffer_index-1) * IO_BUFFER_SIZE));
    _current_buffer_remaining = IO_BUFFER_SIZE;
  }

  inline void free_index(uint64_t index) {
    assert(index <= NUM_IO_BUFFERS);
    _index_ring.mp_enqueue(index);
  }

  void dump_info()
  {
    _hdr.dump_info();
  }

private:
  void post_buffer(unsigned queue_id)
  {
    size_t n_blocks = 0;
    index_t index;
    lba_t lba = _hdr.allocate(IO_BUFFER_SIZE, n_blocks);
    assert(_current_buffer_index <= NUM_IO_BUFFERS);
    assert(_current_buffer_index > 0);
    //PLOG("$$>(%s) @ %ld", (char*)_block->virt_addr(_iob_buffer), lba);

#ifndef DISABLE_IO
    auto wid = _block->async_write(_iob_buffer,
                        (_current_buffer_index-1) * IO_BUFFER_SIZE, /* buffer offset */
                        lba,
                        n_blocks,
                        queue_id, /* queue id */
                        release_buffer,
                        (void*) this,
                        (void*) _current_buffer_index);
    
    _block->check_completion(wid); /* gratuitous completion - will this impact performance? */
#else
    free_index(_current_buffer_index);
#endif

    ready_buffer();
  }

public:
  void flush_buffer()
  {
    size_t n_blocks = 0;
    lba_t lba = _hdr.allocate(IO_BUFFER_SIZE, n_blocks);
    assert(_current_buffer_index <= NUM_IO_BUFFERS);

    /* write zeros to excess tail */
    memset(_current_buffer_ptr, 0,_current_buffer_remaining);
    
    //PLOG("$$>(%s) @ %ld", (char*)_block->virt_addr(_iob_buffer), lba);

#ifndef DISABLE_IO
    _block->write(_iob_buffer,
                  (_current_buffer_index-1) * IO_BUFFER_SIZE, /* buffer offset */
                  lba,
                  n_blocks,
                  0);
#else
    free_index(_current_buffer_index);
#endif
  }


  index_t write_out(uint32_t value, unsigned queue_id)
  {
    auto ds = sizeof(uint32_t);
    if(_current_buffer_remaining == ds) {
      memcpy(_current_buffer_ptr, &value, ds);
      post_buffer(queue_id);
    }
    else if(_current_buffer_remaining > ds) {
      memcpy(_current_buffer_ptr, &value, ds);
      _current_buffer_ptr += ds;
      _current_buffer_remaining -= ds;
    }
    else { /* tail-split */
      unsigned next_seg_len = ds - _current_buffer_remaining;
      byte * ptr = (byte*) &value;
      memcpy(_current_buffer_ptr, ptr, _current_buffer_remaining);
      ptr += _current_buffer_remaining;
      post_buffer(queue_id);
      memcpy(_current_buffer_ptr, ptr, next_seg_len);
      _current_buffer_ptr += next_seg_len;
      assert(next_seg_len > _current_buffer_remaining);
      _current_buffer_remaining -= next_seg_len;
    }
    _shadow_tail += 4;
    return _shadow_tail;
  }
  
  index_t write_out(const void * data, const size_t data_len, unsigned queue_id)
  {    
    if(_current_buffer_remaining == data_len) {
      memcpy(_current_buffer_ptr, data, data_len);
      post_buffer(queue_id);
    }
    else if(_current_buffer_remaining > data_len) {
      memcpy(_current_buffer_ptr, data, data_len);
      _current_buffer_ptr += data_len;
      _current_buffer_remaining -= data_len;
    }
    else { /* tail-split */
      unsigned next_seg_len = data_len - _current_buffer_remaining;
      byte * ptr = (byte*) data;
      memcpy(_current_buffer_ptr, ptr, _current_buffer_remaining);
      ptr += _current_buffer_remaining;
      post_buffer(queue_id);
      memcpy(_current_buffer_ptr, ptr, next_seg_len);
      _current_buffer_ptr += next_seg_len;
      _current_buffer_remaining -= next_seg_len;
    }
    _shadow_tail += data_len;
    return _shadow_tail;
  }


private:
  Component::IBlock_device *  _block;
  unsigned                    _queue_id;
  Header&                     _hdr;
  index_t                     _shadow_tail;
  
  Core::Ring_buffer<uint64_t> _index_ring;
  Component::io_buffer_t      _iob_buffer = 0;
  byte *                      _iob_vaddr = 0;
  uint64_t                    _current_buffer_index = 0;
  byte *                      _current_buffer_ptr = nullptr;
  size_t                      _current_buffer_remaining = 0;
};

#endif
