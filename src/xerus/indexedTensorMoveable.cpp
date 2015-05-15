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

#include <xerus/indexedTensorMoveable.h>

#include <xerus/index.h>
#include <xerus/misc/sfinae.h>
#include <xerus/misc/missingFunctions.h>
#include <xerus/tensor.h>
#include <xerus/tensorNetwork.h>

namespace xerus {
    template<class tensor_type>
    IndexedTensorMoveable<tensor_type>::IndexedTensorMoveable() : IndexedTensorWritable<tensor_type>() { }
    
    template<class tensor_type>
    IndexedTensorMoveable<tensor_type>::IndexedTensorMoveable(IndexedTensorMoveable &&_other ) : IndexedTensorWritable<tensor_type>(std::move(_other)) { }
    
    template<class tensor_type>
    IndexedTensorMoveable<tensor_type>::IndexedTensorMoveable(tensor_type* const _tensorObject, const std::vector<Index>& _indices) : IndexedTensorWritable<tensor_type>(_tensorObject, _indices, true) {}

    template<class tensor_type>
    IndexedTensorMoveable<tensor_type>::IndexedTensorMoveable(tensor_type* const _tensorObject, std::vector<Index>&& _indices) : IndexedTensorWritable<tensor_type>(_tensorObject, std::move(_indices), true) {}

    // IndexedTensorReadOnly may be instanciated as
    template class IndexedTensorMoveable<Tensor>;
    template class IndexedTensorMoveable<TensorNetwork>;
    
}
