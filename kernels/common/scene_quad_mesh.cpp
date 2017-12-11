// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include "scene_quad_mesh.h"
#include "scene.h"

namespace embree
{
#if defined(EMBREE_LOWEST_ISA)

  QuadMesh::QuadMesh (Device* device)
    : Geometry(device,QUAD_MESH,0,1)
  {
    vertices.resize(numTimeSteps);
  }

  void QuadMesh::enabling() 
  { 
    if (numTimeSteps == 1) scene->world.numQuads += numPrimitives;
    else                   scene->worldMB.numQuads += numPrimitives;
  }
  
  void QuadMesh::disabling() 
  { 
    if (numTimeSteps == 1) scene->world.numQuads -= numPrimitives;
    else                   scene->worldMB.numQuads -= numPrimitives;
  }

  void QuadMesh::setMask (unsigned mask) 
  {
    this->mask = mask; 
    Geometry::update();
  }

  void QuadMesh::setSubtype(RTCGeometrySubtype type_in)
  {
    if (type_in != RTC_GEOMETRY_SUBTYPE_SURFACE)
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invalid geometry subtype");
    
    Geometry::update();
  }
  
  void* QuadMesh::newBuffer(RTCBufferType type, size_t stride, unsigned int size) 
  { 
    /* verify that all accesses are 4 bytes aligned */
    if (stride & 0x3) 
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"data must be 4 bytes aligned");

    unsigned bid = type & 0xFFFF;
    if (type >= RTC_VERTEX_BUFFER0 && type < RTC_VERTEX_BUFFER_(RTC_MAX_TIME_STEPS)) 
    {
      /* if buffer is larger than 16GB the premultiplied index optimization does not work */
      if (stride*size > 16ll*1024ll*1024ll*1024ll) 
       throw_RTCError(RTC_ERROR_INVALID_OPERATION,"vertex buffer can be at most 16GB large");

      if (bid >= vertices.size()) vertices.resize(bid+1);
      vertices[bid].newBuffer(device,size,stride); 
      vertices0 = vertices[0];
      setNumTimeSteps((unsigned int)vertices.size());
      return vertices[bid].get();
    } 
    else if (type >= RTC_USER_VERTEX_BUFFER0 && type < RTC_USER_VERTEX_BUFFER0+RTC_MAX_USER_VERTEX_BUFFERS)
    {
      if (bid >= userbuffers.size()) userbuffers.resize(bid+1);
      userbuffers[bid] = Buffer<char>(device,size,stride,true);
      return userbuffers[bid].get();
    }
    else if (type == RTC_INDEX_BUFFER) 
    {
      quads.newBuffer(device,size,stride);
      setNumPrimitives(size);
      return quads.get();
    }
    else
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown buffer type");

    return nullptr;
  }

  void QuadMesh::setBuffer(RTCBufferType type, void* ptr, size_t offset, size_t stride, unsigned int size) 
  { 
    /* verify that all accesses are 4 bytes aligned */
    if (((size_t(ptr) + offset) & 0x3) || (stride & 0x3)) 
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"data must be 4 bytes aligned");

    unsigned bid = type & 0xFFFF;
    if (type >= RTC_VERTEX_BUFFER0 && type < RTC_VERTEX_BUFFER_(RTC_MAX_TIME_STEPS)) 
    {
      /* if buffer is larger than 16GB the premultiplied index optimization does not work */
      if (stride*size > 16ll*1024ll*1024ll*1024ll) 
       throw_RTCError(RTC_ERROR_INVALID_OPERATION,"vertex buffer can be at most 16GB large");

      if (bid >= vertices.size()) vertices.resize(bid+1);
      vertices[bid].set(device,ptr,offset,stride,size); 
      vertices[bid].checkPadding16();
      vertices0 = vertices[0];
      //while (vertices.size() > 1 && vertices.back().getPtr() == nullptr)
      //  vertices.pop_back();
      setNumTimeSteps((unsigned int)vertices.size());
    } 
    else if (type >= RTC_USER_VERTEX_BUFFER0 && type < RTC_USER_VERTEX_BUFFER0+RTC_MAX_USER_VERTEX_BUFFERS)
    {
      if (bid >= userbuffers.size()) userbuffers.resize(bid+1);
      userbuffers[bid] = Buffer<char>(device,size,stride);
      userbuffers[bid].set(device,ptr,offset,stride,size);  
      userbuffers[bid].checkPadding16();
    }
    else if (type == RTC_INDEX_BUFFER) 
    {
      quads.set(device,ptr,offset,stride,size);
      setNumPrimitives(size);
    }
    else
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown buffer type");
  }

  void* QuadMesh::getBuffer(RTCBufferType type) 
  {
    if (type == RTC_INDEX_BUFFER) {
      return quads.get();
    }
    else if (type >= RTC_VERTEX_BUFFER0 && type < RTC_VERTEX_BUFFER_(numTimeSteps)) {
      return vertices[type - RTC_VERTEX_BUFFER0].get();
    }
    else {
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown buffer type"); 
      return nullptr;
    }
  }

  void QuadMesh::preCommit () 
  {
    /* verify that stride of all time steps are identical */
    for (unsigned int t=0; t<numTimeSteps; t++)
      if (vertices[t].getStride() != vertices[0].getStride())
        throw_RTCError(RTC_ERROR_INVALID_OPERATION,"stride of vertex buffers have to be identical for each time step");

    Geometry::preCommit();
  }

  void QuadMesh::postCommit () 
  {
    scene->vertices[geomID] = (int*) vertices0.getPtr();
    Geometry::postCommit();
  }

  bool QuadMesh::verify () 
  {
    /*! verify consistent size of vertex arrays */
    if (vertices.size() == 0) return false;
    for (const auto& buffer : vertices)
      if (buffer.size() != numVertices())
        return false;

    /*! verify quad indices */
    for (size_t i=0; i<size(); i++) {     
      if (quads[i].v[0] >= numVertices()) return false; 
      if (quads[i].v[1] >= numVertices()) return false; 
      if (quads[i].v[2] >= numVertices()) return false; 
      if (quads[i].v[3] >= numVertices()) return false; 
    }

    /*! verify vertices */
    for (const auto& buffer : vertices)
      for (size_t i=0; i<buffer.size(); i++)
	if (!isvalid(buffer[i])) 
	  return false;

    return true;
  }

  void QuadMesh::interpolate(const RTCInterpolateArguments* const args)
  {
    unsigned int primID = args->primID;
    float u = args->u;
    float v = args->v;
    RTCBufferType buffer = args->buffer;
    float* P = args->P;
    float* dPdu = args->dPdu;
    float* dPdv = args->dPdv;
    float* ddPdudu = args->ddPdudu;
    float* ddPdvdv = args->ddPdvdv;
    float* ddPdudv = args->ddPdudv;
    unsigned int numFloats = args->numFloats;

    /* calculate base pointer and stride */
    assert((buffer >= RTC_VERTEX_BUFFER0 && buffer < RTC_VERTEX_BUFFER_(numTimeSteps)) ||
           (buffer >= RTC_USER_VERTEX_BUFFER0 && buffer <= RTC_USER_VERTEX_BUFFER_(RTC_MAX_USER_VERTEX_BUFFERS)));
    const char* src = nullptr; 
    size_t stride = 0;
    if (buffer >= RTC_USER_VERTEX_BUFFER0) {
      src    = userbuffers[buffer&0xFFFF].getPtr();
      stride = userbuffers[buffer&0xFFFF].getStride();
    } else {
      src    = vertices[buffer&0xFFFF].getPtr();
      stride = vertices[buffer&0xFFFF].getStride();
    }

    for (unsigned int i=0; i<numFloats; i+=4)
    {
      const vbool4 valid = vint4((int)i)+vint4(step) < vint4(int(numFloats));
      const size_t ofs = i*sizeof(float);
      const Quad& tri = quad(primID);
      const vfloat4 p0 = vfloat4::loadu(valid,(float*)&src[tri.v[0]*stride+ofs]);
      const vfloat4 p1 = vfloat4::loadu(valid,(float*)&src[tri.v[1]*stride+ofs]);
      const vfloat4 p2 = vfloat4::loadu(valid,(float*)&src[tri.v[2]*stride+ofs]);
      const vfloat4 p3 = vfloat4::loadu(valid,(float*)&src[tri.v[3]*stride+ofs]);      
      const vbool4 left = u+v <= 1.0f;
      const vfloat4 Q0 = select(left,p0,p2);
      const vfloat4 Q1 = select(left,p1,p3);
      const vfloat4 Q2 = select(left,p3,p1);
      const vfloat4 U  = select(left,u,vfloat4(1.0f)-u);
      const vfloat4 V  = select(left,v,vfloat4(1.0f)-v);
      const vfloat4 W  = 1.0f-U-V;
      if (P) {
        vfloat4::storeu(valid,P+i,madd(W,Q0,madd(U,Q1,V*Q2)));
      }
      if (dPdu) { 
        assert(dPdu); vfloat4::storeu(valid,dPdu+i,select(left,Q1-Q0,Q0-Q1));
        assert(dPdv); vfloat4::storeu(valid,dPdv+i,select(left,Q2-Q0,Q0-Q2));
      }
      if (ddPdudu) { 
        assert(ddPdudu); vfloat4::storeu(valid,ddPdudu+i,vfloat4(zero));
        assert(ddPdvdv); vfloat4::storeu(valid,ddPdvdv+i,vfloat4(zero));
        assert(ddPdudv); vfloat4::storeu(valid,ddPdudv+i,vfloat4(zero));
      }
    }
  }
  
#endif

  namespace isa
  {
    QuadMesh* createQuadMesh(Device* device) {
      return new QuadMeshISA(device);
    }
  }
}
