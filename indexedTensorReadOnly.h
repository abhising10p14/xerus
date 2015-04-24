// Xerus - A General Purpose Tensor Library
// Copyright (C) 2014-2015 Benjamin Huber and Sebastian Wolf. 
// 
// Xerus is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
// 
// Xerus is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
// 
// You should have received a copy of the GNU Affero General Public License
// along with Xerus. If not, see <http://www.gnu.org/licenses/>.
//
// For further information on Xerus visit https://libXerus.org 
// or contact us at contact@libXerus.org.

#pragma once

#include "assignedIndices.h"

#define tensor_type_restrictions typename std::enable_if< \
               std::is_same<Tensor,        typename std::decay<tensor_type>::type>{}  \
            || std::is_same<TensorNetwork, typename std::decay<tensor_type>::type>{},  \
        int>::type = 0

namespace xerus {
    // Forward declaration of the two allowed types
    class Tensor;
    class TensorNetwork;
    
	/**
	 * Class representing any Tensor or TensorNetwork object equipped with an Index order, that can at least be read (i.e. is not nessecarily writeable)
	 */
    template<class tensor_type, tensor_type_restrictions>
    class IndexedTensorReadOnly {
    public:
        /// Pointer to the associated Tensor/TensorNetwork object
        const tensor_type* tensorObjectReadOnly;
         
        /// Vector of the associates indices 
        std::vector<Index> indices;
        
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Constructors - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
    protected:
        /// Creates an empty IndexedTensorReadOnly, should only be used internally
        IndexedTensorReadOnly() : tensorObjectReadOnly(nullptr) {};
        
    public:
        /// There is no usefull copy constructor
        IndexedTensorReadOnly(const IndexedTensorReadOnly & _other ) = delete;
        
        /// Move-constructor
        IndexedTensorReadOnly(IndexedTensorReadOnly<tensor_type> && _other ) :
            tensorObjectReadOnly(_other.tensorObjectReadOnly),
            indices(std::move(_other.indices))
            { }
        
        /// Constructs an IndexedTensorReadOnly using the given pointer and indices
        ALLOW_MOVE(std::vector<Index>, VECTOR)
        IndexedTensorReadOnly(const tensor_type* const _tensorObjectReadOnly, VECTOR&& _indices) : tensorObjectReadOnly(_tensorObjectReadOnly), indices(std::forward<VECTOR>(_indices)) { }
        
    public:
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Destructor - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
        
        /// Destructor must be virtual
        virtual ~IndexedTensorReadOnly() { }
        
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Others - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
        bool uses_tensor(const tensor_type *otherTensor) const {
            return otherTensor == tensorObjectReadOnly;
        }
        
        size_t degree() const {
            return tensorObjectReadOnly->degree();
        }
        
        bool is_open(const Index& idx) const {
            REQUIRE(contains(indices, idx), "Index " << idx << " not contained in indices: " << indices);
            return !idx.is_fixed() && count(indices, idx) == 1;
        }
        
        bool is_contained_and_open(const Index& idx) const {
            return !idx.is_fixed() && count(indices, idx) == 1;
        }
        
        std::vector<size_t> get_evaluated_dimensions(const std::vector<Index>& _indexOrder) const {
            std::vector<size_t> evalDimensions;
            evalDimensions.reserve(_indexOrder.size());
            for(const Index& idx : _indexOrder) {
                REQUIRE(count(indices, idx) == 1, "All indices of evaluation target must appear exactly once.");
                
                // Find index
                size_t indexPos = 0, dimCount = 0;
                while(indices[indexPos] != idx) { dimCount += indices[indexPos].inverseSpan ? degree()-indices[indexPos].span : indices[indexPos].span; ++indexPos; }
                
                // Calculate span
                size_t span = indices[indexPos].inverseSpan ? degree()-indices[indexPos].span : indices[indexPos].span;
                
                REQUIRE(dimCount+span <= tensorObjectReadOnly->dimensions.size(), "Order determined by Indices is to large.");
                
                // Insert dimensions
                for(size_t i = 0; i < span; ++i) {
                    evalDimensions.emplace_back(tensorObjectReadOnly->dimensions[dimCount+i]);
                }
            }
            return evalDimensions;
        }
        
        
        AssignedIndices assign_indices() const {
            AssignedIndices assignedIndices;
            
            assignedIndices.allIndicesOpen = true;
            assignedIndices.numIndices = 0;
            assignedIndices.indices.reserve(indices.size());
            assignedIndices.indexDimensions.reserve(indices.size());
            assignedIndices.indexOpen.reserve(indices.size());
            
            size_t dimensionCount = 0;
            for(const Index& idx : indices) {
                REQUIRE(!idx.is_fixed() || idx.span == 1, "Fixed index must have span == 1");
                
                // We don't look at span zero indices
                if((!idx.inverseSpan && idx.span == 0) || (idx.inverseSpan && idx.span == this->degree())) {
                    continue;
                }
                
                // Set span
                size_t span;
                if(idx.inverseSpan) {
                    REQUIRE(idx.span < degree(), "Index used with variable span (e.g. i&3) would have negative span " << (long)(degree() - idx.span) << "!");
                    REQUIRE(!idx.is_fixed(), "Fixed index must not have inverse span");
                    span = degree()-idx.span;
                } else {
                    span = idx.span;
                }
                
                // Calculate multDimension
                REQUIRE(dimensionCount+span <= tensorObjectReadOnly->dimensions.size(), "Order determined by Indices is to large.");
                size_t multDimension = 1;
                for(size_t i=0; i < span; ++i) {
                    multDimension *= tensorObjectReadOnly->dimensions[dimensionCount++];
                }
                
                // Determine whether index is open and 
                bool open;
                if(idx.is_fixed() ) {
                    open = false;
                } else {
                    open = true;
                    for(size_t i = 0; i < assignedIndices.indices.size(); ++i) {
                        if(idx == assignedIndices.indices[i]) {
                            REQUIRE(assignedIndices.indexOpen[i], "An index must not appere more than twice!");
                            assignedIndices.indexOpen[i] = false;
                            open = false;
                        }
                    }
                }
                
                assignedIndices.numIndices++;
                assignedIndices.indices.emplace_back(idx.valueId, span);
                assignedIndices.indexDimensions.emplace_back(multDimension);
                assignedIndices.indexOpen.push_back(open);
                assignedIndices.allIndicesOpen = assignedIndices.allIndicesOpen && open; 
            }
            
            REQUIRE(assignedIndices.numIndices == assignedIndices.indices.size() && assignedIndices.numIndices == assignedIndices.indexDimensions.size() && assignedIndices.numIndices == assignedIndices.indexOpen.size(), "Internal Error"); 
            REQUIRE(dimensionCount >= degree(), "Order determined by Indices is to small. Order according to the indices " << dimensionCount << ", according to the tensor " << degree());
            REQUIRE(dimensionCount <= degree(), "Order determined by Indices is to large. Order according to the indices " << dimensionCount << ", according to the tensor " << degree());
            return assignedIndices;
        }
        
        
        std::vector<Index> get_assigned_indices() const {
            return get_assigned_indices(this->degree());
        }
        
        std::vector<Index> get_assigned_indices(const size_t _futureDegree) const {
            std::vector<Index> assignedIndices;
            assignedIndices.reserve(indices.size());
            
            size_t dimensionCount = 0;
            for(const Index& idx : indices) {
                REQUIRE(!idx.is_fixed() || idx.span == 1, "Fixed index must have span == 1");
                
                // We don't look at span zero indices
                if((!idx.inverseSpan && idx.span == 0) || (idx.inverseSpan && idx.span == _futureDegree)) {
                    continue;
                }
                
                // Set span
                size_t span;
                if(idx.inverseSpan) {
                    REQUIRE(idx.span < _futureDegree, "Index used with variable span (e.g. i&3) would have negative span " << _futureDegree << " - " << idx.span << " = " << (long)(_futureDegree - idx.span) << "!");
                    REQUIRE(!idx.is_fixed(), "Fixed index must not have inverse span");
                    span = _futureDegree-idx.span;
                } else {
                    span = idx.span;
                }
                assignedIndices.emplace_back(idx.valueId, span, false);
                dimensionCount += span;
            }
            
            REQUIRE(dimensionCount >= _futureDegree, "Order determined by Indices is to small. Order according to the indices " << dimensionCount << ", according to the tensor " << _futureDegree);
            REQUIRE(dimensionCount <= _futureDegree, "Order determined by Indices is to large. Order according to the indices " << dimensionCount << ", according to the tensor " << _futureDegree);
            return assignedIndices;
        }
        
        /// Checks whether the indices are usefull in combination with the current dimensions
        void check_indices(const bool _allowNonOpen = true) const {
            #ifdef CHECK_
                size_t dimensionCount = 0;
                for(const Index& idx : indices) {
                    REQUIRE(_allowNonOpen || !idx.is_fixed(), "Fixed indices are not allowed here.");
                    REQUIRE(!idx.is_fixed() || idx.span == 1, "Fixed index must have span == 1");
                    REQUIRE(_allowNonOpen || count(indices, idx) == 1, "Traces are not allowed here.");
                    REQUIRE(idx.is_fixed() || count(indices, idx) <= 2, "An index must not appere more than twice!");
                    
                    if(idx.inverseSpan) {
                        REQUIRE(idx.span <= degree(), "Index used with variable span (e.g. i&3) would have negative span " << (long)(degree() - idx.span) << "!");
                        REQUIRE(!idx.is_fixed(), "Fixed index must not have inverse span");
                        dimensionCount += degree()-idx.span;
                    } else {
                        dimensionCount += idx.span;
                    }
                }
                REQUIRE(dimensionCount >= degree(), "Order determined by Indices is to small. Order according to the indices " << dimensionCount << ", according to the tensor " << degree());
                REQUIRE(dimensionCount <= degree(), "Order determined by Indices is to large. Order according to the indices " << dimensionCount << ", according to the tensor " << degree());
            #endif
        }
        
        
    };

    template<class T>
    _inline_ value_t frob_norm(const IndexedTensorReadOnly<T>& _A) {
        return _A.tensorObjectReadOnly->frob_norm(); 
    }
    
    _inline_ size_t get_eval_degree(const std::vector<Index>& _indices) {
        size_t degree = 0;
        for(const Index& idx : _indices) {
            REQUIRE(!idx.inverseSpan, "Internal Error");
            if(!idx.is_fixed() && count(_indices, idx) != 2) { degree += idx.span; }
        }
        return degree;
    }
}

