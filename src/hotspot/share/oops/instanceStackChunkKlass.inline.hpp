/* Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_OOPS_INSTANCESTACKCHUNKKLASS_INLINE_HPP
#define SHARE_OOPS_INSTANCESTACKCHUNKKLASS_INLINE_HPP

#include "oops/instanceStackChunkKlass.hpp"

#include "classfile/javaClasses.hpp"
#include "code/codeBlob.hpp"
#include "code/codeCache.inline.hpp"
#include "code/nativeInst.hpp"
#include "compiler/oopMap.hpp"
#include "gc/shared/barrierSetNMethod.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/gc_globals.hpp"
#include "logging/log.hpp"
#include "memory/iterator.inline.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/stackChunkOop.inline.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/globals.hpp"
#include "runtime/handles.inline.hpp"
#include "utilities/bitMap.hpp"
#include "utilities/bitMap.inline.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/macros.hpp"

#include CPU_HEADER_INLINE(instanceStackChunkKlass)

#if INCLUDE_ZGC
#include "gc/z/zAddress.inline.hpp"
#define FIX_DERIVED_POINTERS true
#endif
#if INCLUDE_SHENANDOAHGC
#define FIX_DERIVED_POINTERS true
#endif

#ifdef ASSERT
extern "C" bool dbg_is_safe(const void* p, intptr_t errvalue);
extern "C" JNIEXPORT void pns2();
#endif

const int TwoWordAlignmentMask  = (1 << (LogBytesPerWord+1)) - 1;

inline void copy_from_stack_to_chunk(intptr_t* from, intptr_t* to, int size);
inline void copy_from_chunk_to_stack(intptr_t* from, intptr_t* to, int size);

template <bool dword_aligned>
inline void InstanceStackChunkKlass::copy_from_stack_to_chunk(void* from, void* to, size_t size) {
  memcpy(to, from, size << LogBytesPerWord);
#if 0
  if (dword_aligned) {
    assert (size >= 2, ""); // one word for return address, another for rbp spill
    assert(((intptr_t)from & TwoWordAlignmentMask) == 0, "");
    assert(((intptr_t)to   & WordAlignmentMask)    == 0, "");

    memcpy_fn_from_stack_to_chunk(from, to, size);
  } else {
    default_memcpy(from, to, size);
  }
#endif
}

template <bool dword_aligned>
inline void InstanceStackChunkKlass::copy_from_chunk_to_stack(void* from, void* to, size_t size) {
  memcpy(to, from, size << LogBytesPerWord);
#if 0
  if (dword_aligned) {
    assert (size >= 2, ""); // one word for return address, another for rbp spill
    assert(((intptr_t)from & WordAlignmentMask)    == 0, "");
    assert(((intptr_t)to   & TwoWordAlignmentMask) == 0, "");

    memcpy_fn_from_chunk_to_stack(from, to, size);
  } else {
    default_memcpy(from, to, size);
  }
#endif
}

template <bool mixed>
StackChunkFrameStream<mixed>::StackChunkFrameStream(stackChunkOop chunk, bool gc) DEBUG_ONLY(: _chunk(chunk)) {
  assert (chunk->is_stackChunk(), "");
  assert (mixed || !chunk->has_mixed_frames(), "");

  DEBUG_ONLY(_index = 0;)
  _end = chunk->bottom_address();
  _sp = chunk->start_address() + get_initial_sp(chunk, gc);
  assert (_sp <= chunk->end_address() + InstanceStackChunkKlass::metadata_words(), "");

  get_cb();

  if (mixed) {
    if (!is_done() && is_interpreted()) {
      _unextended_sp = unextended_sp_for_interpreter_frame();
    } else {
      _unextended_sp = _sp;
    }
    assert (_unextended_sp >= _sp - InstanceStackChunkKlass::metadata_words(), "");
    // else if (is_compiled()) {
    //   tty->print_cr(">>>>> XXXX"); os::print_location(tty, (intptr_t)nativeCall_before(pc())->destination());
    //   assert (NativeCall::is_call_before(pc()) && nativeCall_before(pc()) != nullptr && nativeCall_before(pc())->destination() != nullptr, "");
    //   if (Interpreter::contains(nativeCall_before(pc())->destination())) { // interpreted callee
    //     _unextended_sp = unextended_sp_for_interpreter_frame_caller();
    //   }
    // }
  }
  DEBUG_ONLY(else _unextended_sp = nullptr;)
  
  if (is_stub()) {
    get_oopmap(pc(), 0);
    DEBUG_ONLY(_has_stub = true);
  } DEBUG_ONLY(else _has_stub = false;)
}

template <bool mixed>
StackChunkFrameStream<mixed>::StackChunkFrameStream(stackChunkOop chunk, const frame& f) DEBUG_ONLY(: _chunk(chunk)) {
  assert (chunk->is_stackChunk(), "");
  assert (mixed || !chunk->has_mixed_frames(), "");

  DEBUG_ONLY(_index = 0;)
  
  _end = chunk->bottom_address();

  assert (chunk->is_in_chunk(f.sp()), "");
  _sp = f.sp();
  if (mixed) {
    _unextended_sp = f.unextended_sp();
    assert (_unextended_sp >= _sp - InstanceStackChunkKlass::metadata_words(), "");
  }
  DEBUG_ONLY(else _unextended_sp = nullptr;)
  assert (_sp >= chunk->start_address() && _sp <= chunk->end_address() + InstanceStackChunkKlass::metadata_words(), "");

  if (f.cb() != nullptr) {
    _oopmap = nullptr;
    _cb = f.cb();
  } else {
    get_cb();
  }

  if (is_stub()) {
    get_oopmap(pc(), 0);
    DEBUG_ONLY(_has_stub = true);
  } DEBUG_ONLY(else _has_stub = false;)
}

template <bool mixed>
inline bool StackChunkFrameStream<mixed>::is_stub() const {
  return cb() != nullptr && (_cb->is_safepoint_stub() || _cb->is_runtime_stub());
}

template <bool mixed>
inline bool StackChunkFrameStream<mixed>::is_compiled() const {
  return cb() != nullptr && _cb->is_compiled();
}

template <bool mixed>
inline bool StackChunkFrameStream<mixed>::is_interpreted() const { 
  return mixed ? (!is_done() && Interpreter::contains(pc())) : false; 
}

template <bool mixed>
inline int StackChunkFrameStream<mixed>::frame_size() const {
  return is_interpreted() ? interpreter_frame_size() 
                          : cb()->frame_size() + stack_argsize();
}

template <bool mixed>
inline int StackChunkFrameStream<mixed>::stack_argsize() const {
  if (is_interpreted()) return interpreter_frame_stack_argsize();
  if (is_stub()) return 0;
  guarantee (cb() != nullptr, "");
  guarantee (cb()->is_compiled(), "");
  guarantee (cb()->as_compiled_method()->method() != nullptr, "");
  return (cb()->as_compiled_method()->method()->num_stack_arg_slots() * VMRegImpl::stack_slot_size) >> LogBytesPerWord;
}

template <bool mixed>
inline int StackChunkFrameStream<mixed>::num_oops() const {
  return is_interpreted() ? interpreter_frame_num_oops() : oopmap()->num_oops();
}

template <bool mixed>
inline void StackChunkFrameStream<mixed>::initialize_register_map(RegisterMap* map) {
  update_reg_map_pd(map);
}

template <bool mixed>
template <typename RegisterMapT>
inline void StackChunkFrameStream<mixed>::next(RegisterMapT* map) {
  update_reg_map(map);
  bool safepoint = is_stub();
  if (mixed) {
    if (is_interpreted()) next_for_interpreter_frame();
    else {
      _sp = _unextended_sp + cb()->frame_size();
      if (_sp >= _end - InstanceStackChunkKlass::metadata_words()) {
        _sp = _end;
      }
      _unextended_sp = is_interpreted() ? unextended_sp_for_interpreter_frame() : _sp;
    }
    assert (_unextended_sp >= _sp - InstanceStackChunkKlass::metadata_words(), "");
  } else {
    _sp += cb()->frame_size();
  }
  assert (!is_interpreted() || _unextended_sp == unextended_sp_for_interpreter_frame(), "_unextended_sp: " INTPTR_FORMAT " unextended_sp: " INTPTR_FORMAT, p2i(_unextended_sp), p2i(unextended_sp_for_interpreter_frame()));

  get_cb();
  update_reg_map_pd(map);
  if (safepoint && cb() != nullptr) _oopmap = cb()->oop_map_for_return_address(pc()); // there's no post-call nop and no fast oopmap lookup
  DEBUG_ONLY(_index++;)
}

template <bool mixed>
inline intptr_t* StackChunkFrameStream<mixed>::next_sp() const {
  return is_interpreted() ? next_sp_for_interpreter_frame() : unextended_sp() + cb()->frame_size();
}

template <bool mixed>
inline void StackChunkFrameStream<mixed>::get_cb() {
  _oopmap = nullptr;
  if (is_done() || is_interpreted()) {
    _cb = nullptr;
    return;
  }

  assert (pc() != nullptr && dbg_is_safe(pc(), -1), 
  "index: %d sp: " INTPTR_FORMAT " sp offset: %d end offset: %d size: %d chunk sp: %d", 
  _index, p2i(sp()), _chunk->to_offset(sp()), _chunk->to_offset(_chunk->bottom_address()), _chunk->stack_size(), _chunk->sp());

  _cb = CodeCache::find_blob_fast(pc());

  // if (_cb == nullptr) { tty->print_cr("OOPS"); os::print_location(tty, (intptr_t)pc()); }
  assert (_cb != nullptr, 
    "index: %d sp: " INTPTR_FORMAT " sp offset: %d end offset: %d size: %d chunk sp: %d gc_flag: %d", 
    _index, p2i(sp()), _chunk->to_offset(sp()), _chunk->to_offset(_chunk->bottom_address()), _chunk->stack_size(), _chunk->sp(), _chunk->is_gc_mode());
  assert (is_interpreted() || ((is_stub() || is_compiled()) && _cb->frame_size() > 0), 
    "index: %d sp: " INTPTR_FORMAT " sp offset: %d end offset: %d size: %d chunk sp: %d is_stub: %d is_compiled: %d frame_size: %d mixed: %d", 
    _index, p2i(sp()), _chunk->to_offset(sp()), _chunk->to_offset(_chunk->bottom_address()), _chunk->stack_size(), _chunk->sp(), is_stub(), is_compiled(), _cb->frame_size(), mixed);
}

template <bool mixed>
inline void StackChunkFrameStream<mixed>::get_oopmap() const {
  if (is_interpreted()) return;
  assert (is_compiled(), "");
  get_oopmap(pc(), CodeCache::find_oopmap_slot_fast(pc())); 
}

template <bool mixed>
inline void StackChunkFrameStream<mixed>::get_oopmap(address pc, int oopmap_slot) const {
  assert (cb() != nullptr, "");
  assert (!is_compiled() || !cb()->as_compiled_method()->is_deopt_pc(pc), "oopmap_slot: %d", oopmap_slot);
  if (oopmap_slot >= 0) {
    assert (oopmap_slot >= 0, "");
    assert (cb()->oop_map_for_slot(oopmap_slot, pc) != nullptr, "");
    assert (cb()->oop_map_for_slot(oopmap_slot, pc) == cb()->oop_map_for_return_address(pc), "");

    _oopmap = cb()->oop_map_for_slot(oopmap_slot, pc);
  } else {
    _oopmap = cb()->oop_map_for_return_address(pc);
  }
  assert (_oopmap != nullptr, "");
}

template <bool mixed>
inline int StackChunkFrameStream<mixed>::get_initial_sp(stackChunkOop chunk, bool gc) {
  int chunk_sp = chunk->sp();
  // we don't invoke write barriers on oops in thawed frames, so we use the gcSP field to traverse thawed frames
  // if (gc && chunk_sp != chunk->gc_sp() && chunk->requires_barriers()) {
  //   uint64_t marking_cycle = CodeCache::marking_cycle() >> 1;
  //   uint64_t chunk_marking_cycle = chunk->mark_cycle() >> 1;
  //   if (marking_cycle == chunk_marking_cycle) {
  //     // Marking isn't finished, so we need to traverse thawed frames
  //     chunk_sp = chunk->gc_sp();
  //     assert (chunk_sp >= 0 && chunk_sp <= chunk->sp(), "");
  //   } else {
  //     chunk->set_gc_sp(chunk_sp); // atomic; benign race
  //   }
  // }
  assert (chunk_sp >= 0, "");
  return chunk_sp;
}

template <bool mixed>
template <typename RegisterMapT>
inline void* StackChunkFrameStream<mixed>::reg_to_loc(VMReg reg, const RegisterMapT* map) const {
  assert (!is_done(), "");
  return reg->is_reg() ? (void*)map->location(reg, sp()) // see frame::update_map_with_saved_link(&map, link_addr);
                       : (void*)((address)unextended_sp() + (reg->reg2stack() * VMRegImpl::stack_slot_size));
}

template<>
template<>
inline void StackChunkFrameStream<true>::update_reg_map(RegisterMap* map) {
  assert (!map->in_cont() || map->stack_chunk() == _chunk, "");
  if (map->update_map() && is_stub()) {
    frame f = to_frame();
    oopmap()->update_register_map(&f, map); // we have callee-save registers in this case
  }
}

template<>
template<>
inline void StackChunkFrameStream<false>::update_reg_map(RegisterMap* map) {
  assert (map->in_cont() && map->stack_chunk()() == _chunk, "");
  if (map->update_map()) {
    frame f = to_frame();
    oopmap()->update_register_map(&f, map); // we have callee-save registers in this case
  }
}

template <bool mixed>
template <typename RegisterMapT>
inline void StackChunkFrameStream<mixed>::update_reg_map(RegisterMapT* map) {}

template <bool mixed>
inline address  StackChunkFrameStream<mixed>::orig_pc() const {
  address pc1 = pc();
  if (is_interpreted() || is_stub()) return pc1;
  CompiledMethod* cm = cb()->as_compiled_method();
  if (cm->is_deopt_pc(pc1)) {
    pc1 = *(address*)((address)unextended_sp() + cm->orig_pc_offset());
  }

  assert (pc1 != nullptr && !cm->is_deopt_pc(pc1),
          "index: %d sp - start: " INTPTR_FORMAT " end - sp: " INTPTR_FORMAT " size: %d sp: %d",
          _index, sp() - _chunk->sp_address(), end() - sp(), _chunk->stack_size(), _chunk->sp());
  assert (_cb == CodeCache::find_blob_fast(pc1), "");

  return pc1;
}

#ifdef ASSERT
template <bool mixed>
bool StackChunkFrameStream<mixed>::is_deoptimized() const {
  address pc1 = pc();
  return is_compiled() && CodeCache::find_oopmap_slot_fast(pc1) < 0 && cb()->as_compiled_method()->is_deopt_pc(pc1);
}
#endif

template <bool mixed>
void StackChunkFrameStream<mixed>::handle_deopted() const {
  assert (!is_done(), "");

  if (_oopmap != nullptr) return;
  if (is_interpreted()) return;
  assert (is_compiled(), "");

  address pc1 = pc();
  int oopmap_slot = CodeCache::find_oopmap_slot_fast(pc1);
  if (UNLIKELY(oopmap_slot < 0)) { // we could have marked frames for deoptimization in thaw_chunk
    if (cb()->as_compiled_method()->is_deopt_pc(pc1)) {
      pc1 = orig_pc();
      oopmap_slot = CodeCache::find_oopmap_slot_fast(pc1);
    }
  }
  get_oopmap(pc1, oopmap_slot);
}

template <bool mixed>
template <class OopClosureType, class RegisterMapT>
inline void StackChunkFrameStream<mixed>::iterate_oops(OopClosureType* closure, const RegisterMapT* map) const {
  if (is_interpreted()) {
    frame f = to_frame();
    // InterpreterOopMap mask;
    // f.interpreted_frame_oop_map(&mask);
    f.oops_interpreted_do<true>(closure, nullptr, true);
  } else {
    DEBUG_ONLY(int oops = 0;)
    for (OopMapStream oms(oopmap()); !oms.is_done(); oms.next()) { // see void OopMapDo<OopFnT, DerivedOopFnT, ValueFilterT>::iterate_oops_do
      OopMapValue omv = oms.current();
      if (omv.type() != OopMapValue::oop_value && omv.type() != OopMapValue::narrowoop_value)
        continue;

      assert (UseCompressedOops || omv.type() == OopMapValue::oop_value, "");
      DEBUG_ONLY(oops++;)

      void* p = reg_to_loc(omv.reg(), map);
      assert (p != nullptr, "");
      assert ((_has_stub && _index == 1) || is_in_frame(p), "");

      // if ((intptr_t*)p >= end) continue; // we could be walking the bottom frame's stack-passed args, belonging to the caller

      // if (!SkipNullValue::should_skip(*p))
      log_develop_trace(jvmcont)("StackChunkFrameStream::iterate_oops narrow: %d reg: %s p: " INTPTR_FORMAT " sp offset: " INTPTR_FORMAT, omv.type() == OopMapValue::narrowoop_value, omv.reg()->name(), p2i(p), (intptr_t*)p - sp());
      omv.type() == OopMapValue::narrowoop_value ? Devirtualizer::do_oop(closure, (narrowOop*)p) : Devirtualizer::do_oop(closure, (oop*)p);
    }
    assert (oops == oopmap()->num_oops(), "oops: %d oopmap->num_oops(): %d", oops, oopmap()->num_oops());
  }
}

template<bool mixed>
template <class DerivedOopClosureType, class RegisterMapT>
inline void StackChunkFrameStream<mixed>::iterate_derived_pointers(DerivedOopClosureType* closure, const RegisterMapT* map) const {
  if (is_interpreted()) return;
  
  for (OopMapStream oms(oopmap()); !oms.is_done(); oms.next()) {
    OopMapValue omv = oms.current();
    if (omv.type() != OopMapValue::derived_oop_value)
      continue;
    
    intptr_t* derived_loc = (intptr_t*)reg_to_loc(omv.reg(), map);
    intptr_t* base_loc    = (intptr_t*)reg_to_loc(omv.content_reg(), map); // see OopMapDo<OopMapFnT, DerivedOopFnT, ValueFilterT>::walk_derived_pointers1

    assert ((_has_stub && _index == 1) || is_in_frame(base_loc), "");
    assert ((_has_stub && _index == 1) || is_in_frame(derived_loc), "");
    assert (derived_loc != base_loc, "Base and derived in same location");
    assert (is_in_oops(base_loc, map), "not found: " INTPTR_FORMAT, p2i(base_loc));
    assert (!is_in_oops(derived_loc, map), "found: " INTPTR_FORMAT, p2i(derived_loc));
    
    Devirtualizer::do_derived_oop(closure, (oop*)base_loc, (derived_pointer*)derived_loc);
  }
  OrderAccess::storestore(); // to preserve that we set the offset *before* fixing the base oop
}

#ifdef ASSERT

template <bool mixed>
template <typename RegisterMapT>
bool StackChunkFrameStream<mixed>::is_in_oops(void* p, const RegisterMapT* map) const {
  for (OopMapStream oms(oopmap()); !oms.is_done(); oms.next()) {
    if (oms.current().type() != OopMapValue::oop_value)
      continue;
    if (reg_to_loc(oms.current().reg(), map) == p)
      return true;
  }
  return false;
}
#endif

#ifdef ASSERT
void InstanceStackChunkKlass::assert_mixed_correct(stackChunkOop chunk, bool mixed) {
  assert (!chunk->has_mixed_frames() || mixed, "has mixed frames: %d mixed: %d", chunk->has_mixed_frames(), mixed);
}
#endif

inline size_t InstanceStackChunkKlass::instance_size(size_t stack_size_in_words) const {
  return align_object_size(size_helper() + stack_size_in_words + bitmap_size(stack_size_in_words));
}

inline HeapWord* InstanceStackChunkKlass::start_of_bitmap(oop obj) {
  return start_of_stack(obj) + jdk_internal_vm_StackChunk::size(obj);
}

inline size_t InstanceStackChunkKlass::bitmap_size(size_t stack_size_in_words) {
  if (!UseChunkBitmaps) return 0;
  size_t size_in_bits = bitmap_size_in_bits(stack_size_in_words);
  static const size_t mask = BitsPerWord - 1;
  int remainder = (size_in_bits & mask) != 0 ? 1 : 0;
  size_t res = (size_in_bits >> LogBitsPerWord) + remainder;
  assert (size_in_bits + bit_offset(stack_size_in_words) == (res << LogBitsPerWord), "size_in_bits: %zu bit_offset: %d res << LogBitsPerWord: %zu", size_in_bits, (int)bit_offset(stack_size_in_words), (res << LogBitsPerWord));
  return res;
}

inline BitMap::idx_t InstanceStackChunkKlass::bit_offset(size_t stack_size_in_words) {
  static const size_t mask = BitsPerWord - 1;
  // tty->print_cr(">>> BitsPerWord: %d MASK: %d stack_size_in_words: %d stack_size_in_words & mask: %d", BitsPerWord, mask, stack_size_in_words, stack_size_in_words & mask);
  return (BitMap::idx_t)((BitsPerWord - (bitmap_size_in_bits(stack_size_in_words) & mask)) & mask);
}

template <typename T, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate(oop obj, OopClosureType* closure) {
  assert (obj->is_stackChunk(), "");
  stackChunkOop chunk = (stackChunkOop)obj;
  if (Devirtualizer::do_metadata(closure)) {
    Devirtualizer::do_klass(closure, this);
  }
  (UseZGC || UseShenandoahGC)
    ? oop_oop_iterate_stack<true,  OopClosureType>(chunk, closure)
    : oop_oop_iterate_stack<false, OopClosureType>(chunk, closure);
  oop_oop_iterate_header<T>(chunk, closure);
}

template <typename T, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_reverse(oop obj, OopClosureType* closure) {
  assert (obj->is_stackChunk(), "");
  assert(!Devirtualizer::do_metadata(closure), "Code to handle metadata is not implemented");
  stackChunkOop chunk = (stackChunkOop)obj;
  (UseZGC || UseShenandoahGC)
    ? oop_oop_iterate_stack<true,  OopClosureType>(chunk, closure)
    : oop_oop_iterate_stack<false, OopClosureType>(chunk, closure);
  oop_oop_iterate_header<T>(chunk, closure);
}

template <typename T, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_bounded(oop obj, OopClosureType* closure, MemRegion mr) {
  assert (obj->is_stackChunk(), "");
  stackChunkOop chunk = (stackChunkOop)obj;
  if (Devirtualizer::do_metadata(closure)) {
    if (mr.contains(obj)) {
      Devirtualizer::do_klass(closure, this);
    }
  }
  // InstanceKlass::oop_oop_iterate_bounded<T>(obj, closure, mr);
  oop_oop_iterate_stack_bounded<false>(chunk, closure, mr);
  oop_oop_iterate_header_bounded<T>(chunk, closure, mr);
}

template <typename T, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_header(stackChunkOop chunk, OopClosureType* closure) {
  T* parent_addr = chunk->field_addr<T>(jdk_internal_vm_StackChunk::parent_offset());
  T* cont_addr = chunk->field_addr<T>(jdk_internal_vm_StackChunk::cont_offset());
  OrderAccess::storestore();
  Devirtualizer::do_oop(closure, parent_addr);
  OrderAccess::storestore();
  Devirtualizer::do_oop(closure, cont_addr); // must be last oop iterated
}

template <typename T, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_header_bounded(stackChunkOop chunk, OopClosureType* closure, MemRegion mr) {
  T* parent_addr = chunk->field_addr<T>(jdk_internal_vm_StackChunk::parent_offset());
  T* cont_addr = chunk->field_addr<T>(jdk_internal_vm_StackChunk::cont_offset());
  if (mr.contains(parent_addr)) {
    OrderAccess::storestore();
    Devirtualizer::do_oop(closure, parent_addr);
  }
  if (mr.contains(cont_addr)) {
    OrderAccess::storestore();
    Devirtualizer::do_oop(closure, cont_addr); // must be last oop iterated
  }
}

template <bool concurrent_gc, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_stack_bounded(stackChunkOop chunk, OopClosureType* closure, MemRegion mr) {
  if (LIKELY(chunk->has_bitmap())) {
    intptr_t* start = chunk->sp_address() - metadata_words();
    intptr_t* end = chunk->end_address();
    // mr.end() can actually be less than start. In that case, we only walk the metadata
    if ((intptr_t*)mr.start() > start) start = (intptr_t*)mr.start();
    if ((intptr_t*)mr.end()   < end)   end   = (intptr_t*)mr.end();
    oop_oop_iterate_stack_helper(chunk, closure, start, end);
  } else {
    oop_oop_iterate_stack_slow<concurrent_gc>(chunk, closure, mr);
  }
}

template <bool concurrent_gc, class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_stack(stackChunkOop chunk, OopClosureType* closure) {
  if (LIKELY(chunk->has_bitmap())) {
    oop_oop_iterate_stack_helper(chunk, closure, chunk->sp_address() - metadata_words(), chunk->end_address());
  } else {
    oop_oop_iterate_stack_slow<concurrent_gc>(chunk, closure, chunk->range());
  }
}

template <typename OopT, typename OopClosureType>
class StackChunkOopIterateBitmapClosure : public BitMapClosure {
  stackChunkOop _chunk;
  OopClosureType* const _closure;
public:
  StackChunkOopIterateBitmapClosure(stackChunkOop chunk, OopClosureType* closure) : _chunk(chunk), _closure(closure) {}
  bool do_bit(BitMap::idx_t index) override {
    Devirtualizer::do_oop(_closure, _chunk->address_for_bit<OopT>(index));
    return true;
  }
};

template <class OopClosureType>
void InstanceStackChunkKlass::oop_oop_iterate_stack_helper(stackChunkOop chunk, OopClosureType* closure, intptr_t* start, intptr_t* end) {
  if (Devirtualizer::do_metadata(closure)) {
    mark_methods(chunk, closure);
  }

  if (end > start) {
    if (UseCompressedOops) {
      StackChunkOopIterateBitmapClosure<narrowOop, OopClosureType> bitmap_closure(chunk, closure);
      chunk->bitmap().iterate(&bitmap_closure, chunk->bit_index_for((narrowOop*)start), chunk->bit_index_for((narrowOop*)end));
    } else {
      StackChunkOopIterateBitmapClosure<oop, OopClosureType> bitmap_closure(chunk, closure);
      chunk->bitmap().iterate(&bitmap_closure, chunk->bit_index_for((oop*)start), chunk->bit_index_for((oop*)end));
    }
  }
}

template <bool mixed, class StackChunkFrameClosureType>
inline void InstanceStackChunkKlass::iterate_stack(stackChunkOop obj, StackChunkFrameClosureType* closure) {
  // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " mixed: %d", p2i(this), mixed);

  const SmallRegisterMap* map = SmallRegisterMap::instance;
  assert (!map->in_cont(), "");

  StackChunkFrameStream<mixed> f(obj);
  // if (f.end() > h) {
  //   // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " bounded", p2i(this));
  //   f.set_end(h);
  // }
  bool should_continue = true;

  if (f.is_stub()) {
    // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " safepoint yield stub frame: %d", p2i(this), f.index());
    // if (log_develop_is_enabled(Trace, jvmcont)) f.print_on(tty);

    RegisterMap full_map((JavaThread*)nullptr, true, false, true);
    full_map.set_include_argument_oops(false);
    
    f.next(&full_map);

    // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " safepoint yield caller frame: %d", p2i(this), f.index());

    assert (!f.is_done(), "");
    assert (f.is_compiled(), "");

    // if (f.sp() + f.frame_size() >= l) {
      // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " stub-caller frame: %d", p2i(this), f.index());
      // if (log_develop_is_enabled(Trace, jvmcont)) f.print_on(tty);

      should_continue = closure->template do_frame<mixed>((const StackChunkFrameStream<mixed>&)f, &full_map);
    // }
    f.next(map);
  }
  assert (!f.is_stub(), "");

  for(; should_continue && !f.is_done(); f.next(map)) {
    // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " frame: %d interpreted: %d", p2i(this), f.index(), f.is_interpreted());
    // if (log_develop_is_enabled(Trace, jvmcont)) f.print_on(tty);
    if (mixed) f.handle_deopted(); // in slow mode we might freeze deoptimized frames
    should_continue = closure->template do_frame<mixed>((const StackChunkFrameStream<mixed>&)f, map);
    // if (!should_continue) log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " stop", p2i(this));
  }
  // log_develop_trace(jvmcont)("stackChunkOopDesc::iterate_stack this: " INTPTR_FORMAT " done index: %d", p2i(this), f.index());
}

#endif // SHARE_OOPS_INSTANCESTACKCHUNKKLASS_INLINE_HPP
