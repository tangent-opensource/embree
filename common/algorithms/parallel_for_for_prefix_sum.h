// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "parallel_for_for.h"
#include "parallel_prefix_sum.h"

namespace embree
{
  template<typename Value>
    struct ParallelForForPrefixSumState : public ParallelForForState
  {
    __forceinline ParallelForForPrefixSumState () {}

    template<typename ArrayArray>
      __forceinline ParallelForForPrefixSumState (ArrayArray& array2, const size_t minStepSize)
      : ParallelForForState(array2,minStepSize) {}

    ParallelPrefixSumState<Value> prefix_state;
  };
  
  template<typename ArrayArray, typename Index, typename Value, typename Func, typename Reduction>
    __forceinline Value parallel_for_for_prefix_sum0( ParallelForForPrefixSumState<Value>& state, ArrayArray& array2, Index minStepSize, 
                                                      const Value& identity, const Func& func, const Reduction& reduction)
  {
    /* calculate number of tasks to use */
    const size_t taskCount = state.taskCount;
    /* perform parallel prefix sum */
    parallel_for(taskCount, [&](const size_t taskIndex)
    {
      const size_t k0 = (taskIndex+0)*state.size()/taskCount;
      const size_t k1 = (taskIndex+1)*state.size()/taskCount;
      size_t i0 = state.i0[taskIndex];
      size_t j0 = state.j0[taskIndex];

      /* iterate over arrays */
      size_t k=k0;
      Value N=identity;
      for (size_t i=i0; k<k1; i++) {
	const size_t size = array2[i] ? array2[i]->size() : 0;
        const size_t r0 = j0, r1 = min(size,r0+k1-k);
        if (r1 > r0) N = reduction(N, func(array2[i],range<size_t>(r0,r1),k,i));
        k+=r1-r0; j0 = 0;
      }
      state.prefix_state.counts[taskIndex] = N;
    });

    /* calculate prefix sum */
    Value sum=identity;
    for (size_t i=0; i<taskCount; i++)
    {
      const Value c = state.prefix_state.counts[i];
      state.prefix_state.sums[i] = sum;
      sum=reduction(sum,c);
    }

    return sum;
  }

  template<typename ArrayArray, typename Index, typename Value, typename Func, typename Reduction>
    __forceinline Value parallel_for_for_prefix_sum1( ParallelForForPrefixSumState<Value>& state, ArrayArray& array2, Index minStepSize, 
                                                      const Value& identity, const Func& func, const Reduction& reduction)
  {
    /* calculate number of tasks to use */
    const size_t taskCount = state.taskCount;
    /* perform parallel prefix sum */
    parallel_for(taskCount, [&](const size_t taskIndex)
    {
      const size_t k0 = (taskIndex+0)*state.size()/taskCount;
      const size_t k1 = (taskIndex+1)*state.size()/taskCount;
      size_t i0 = state.i0[taskIndex];
      size_t j0 = state.j0[taskIndex];

      /* iterate over arrays */
      size_t k=k0;
      Value N=identity;
      for (size_t i=i0; k<k1; i++) {
	const size_t size = array2[i] ? array2[i]->size() : 0;
        const size_t r0 = j0, r1 = min(size,r0+k1-k);
        if (r1 > r0) N = reduction(N, func(array2[i],range<size_t>(r0,r1),k,i,reduction(state.prefix_state.sums[taskIndex],N)));
        k+=r1-r0; j0 = 0;
      }
      state.prefix_state.counts[taskIndex] = N;
    });

    /* calculate prefix sum */
    Value sum=identity;
    for (size_t i=0; i<taskCount; i++)
    {
      const Value c = state.prefix_state.counts[i];
      state.prefix_state.sums[i] = sum;
      sum=reduction(sum,c);
    }

    return sum;
  }

  template<typename ArrayArray, typename Value, typename Func, typename Reduction>
    __forceinline Value parallel_for_for_prefix_sum0( ParallelForForPrefixSumState<Value>& state, ArrayArray& array2, 
						     const Value& identity, const Func& func, const Reduction& reduction)
  {
    return parallel_for_for_prefix_sum0(state,array2,size_t(1),identity,func,reduction);
  }

  template<typename ArrayArray, typename Value, typename Func, typename Reduction>
    __forceinline Value parallel_for_for_prefix_sum1( ParallelForForPrefixSumState<Value>& state, ArrayArray& array2, 
						     const Value& identity, const Func& func, const Reduction& reduction)
  {
    return parallel_for_for_prefix_sum1(state,array2,size_t(1),identity,func,reduction);
  }
}
