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

#include "tensorNode.h"
#include "indexedTensor.h"
#include <map>
#include <set>


namespace xerus {
    // Necessary forward declaritons
    class FullTensor;
    class SparseTensor;
    
    class TensorNetwork {
    public:
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Member variables - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
            
        /// Dimensions of the external indices, i.e. the dimensions of the tensor represented by the network.
        std::vector<size_t> dimensions;
        
        /// The nodes constituting the network. The order determines the ids of the nodes.
		std::vector<TensorNode> nodes;
            
        /// The open links of the network in order.
        std::vector<TensorNode::Link> externalLinks;
        
        /// A single value representing a constant factor and/or the only entry of an order zero tensor
        value_t factor;
        
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Constructors - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
            
        /// Constructs an empty tensor network
		explicit TensorNetwork();
        
        /// Copy Constructor
        implicit TensorNetwork(const TensorNetwork& _cpy);
        
        /// Move Constructor
        implicit TensorNetwork(TensorNetwork&& _mv);
        
        /// Constructs the trivial network containing the given Tensor as single node
        implicit TensorNetwork(const Tensor& _other);
        
        /// Constructs the trivial network containing the given Tensor as single node
        implicit TensorNetwork(Tensor&& _other);
        
        /// Constructs the trivial network containing the given FullTensor
        implicit TensorNetwork(std::unique_ptr<Tensor>&&  _tensor);
        
        /// Constructs the trivial network containing non-specified size-1 fulltensor 
        implicit TensorNetwork(size_t _degree);
        
        /// Destructor
		virtual ~TensorNetwork() {}
        
        /// Returns a new copy of the network.
		virtual TensorNetwork* get_copy() const;
            
    private:
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Internal Helper functions - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
		std::vector<TensorNode::Link> init_from_dimension_array();
        
        /// Checks whether there is a non-trivial global scaling factor, i.e. check factor != 1.0.
        bool has_factor() const;
        
        virtual void apply_factor();
        
        /// Contracts all parts of the network that miss every connection to the external indices.
        void contract_unconnected_subnetworks();

    public:
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Standard operators - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
            
        /// Allows explicit casts to FullTensor
        explicit operator FullTensor() const;
        
        /// Allows explicit casts to SparseTensor
        explicit operator SparseTensor() const;
            
        /// Fully contracts the network to a single tensor and returns it as a unique_ptr. Result can be both full or sparse.
        std::unique_ptr<Tensor> fully_contracted_tensor() const;
        
        /// TensorNetworks are copy assignable.
		TensorNetwork &operator=(const TensorNetwork &_cpy);
            
        /// TensorNetworks are move assignable.
		TensorNetwork &operator=(TensorNetwork &&_mv);
            
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Access - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
        /// Allows read access to the entry at _position, assuming row-major ordering and a single node.
        value_t operator[](const size_t _position) const;
        
        /// Allows access to the entry at _position.
        value_t operator[](const std::vector<size_t>& _positions) const;
        
        
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Indexing - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
		template<typename... args>
		IndexedTensor<TensorNetwork> operator()(args... _args) {
				return IndexedTensor<TensorNetwork>(this, std::vector<Index>({_args...}), false);
		}
		
		template<typename... args>
		IndexedTensorReadOnly<TensorNetwork> operator()(args... _args) const {
				return IndexedTensorReadOnly<TensorNetwork>(this, std::vector<Index>({_args...}));
		}
		
		IndexedTensor<TensorNetwork> operator()(const std::vector<Index> & _indices);
        
        IndexedTensor<TensorNetwork> operator()(      std::vector<Index>&& _indices);
            
		IndexedTensorReadOnly<TensorNetwork> operator()(const std::vector<Index> & _indices) const;
        
        IndexedTensorReadOnly<TensorNetwork> operator()(      std::vector<Index>&& _indices) const;
            
		/*- - - - - - - - - - - - - - - - - - - - - - - - - - Operator specializations - - - - - - - - - - - - - - - - - - - - - - - - - - */
		virtual bool specialized_contraction(IndexedTensorWritable<TensorNetwork> &_out, const IndexedTensorReadOnly<TensorNetwork> &_me, const IndexedTensorReadOnly<TensorNetwork> &_other) const;
        
		virtual bool specialized_sum(IndexedTensorWritable<TensorNetwork> &_out, const IndexedTensorReadOnly<TensorNetwork> &_me, const IndexedTensorReadOnly<TensorNetwork> &_other) const;
        
		virtual void specialized_evaluation(const IndexedTensorWritable<TensorNetwork> &_me, const IndexedTensorReadOnly<TensorNetwork> &_other);
            
            
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - Miscellaneous - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
        size_t degree() const;
        
        /// Eleminates all erased Nodes
        void sanitize();
            
		/// reshuffled the nodes according to the given (from, to) map
		void reshuffle_nodes(const std::map<size_t, size_t> &_map);
		
		/// reshuffled the nodes according to the given function
		void reshuffle_nodes(std::function<size_t(size_t)> _f);
		
		/// check whether all links in the network are set consistently and matching the underlying tensor objects
		bool is_valid_network() const;
        
        /// Creates a copy of a subnet that only contains nullptr as data pointers
        TensorNetwork stripped_subnet(std::set<size_t> _ids) const;
        
        void swap_external_links(const size_t _i, const size_t _j);
        
        /// shuffles the external links of _lhs according to the indices of the indexedTensors
        /// lhs contains a copy of rhs, thus we have to swap the rhs.indices to resemble those of the lhs
		static void shuffle_indices(std::vector<Index> &_currentIndices, const IndexedTensorWritable<TensorNetwork> &_lhs);
		
		static void add_network_to_network(IndexedTensorWritable<TensorNetwork> & _base, const IndexedTensorReadOnly<TensorNetwork> & _toInsert);
		static void trace_out_double_indices(std::vector<Index> &_modifiedIndices, const IndexedTensorWritable<TensorNetwork> & _base);
	
		/**
		* contracts the nodes with indices @a _node1 and @a _node2
		* replaces node1 with the contraction and node2 with an degree-0 tensor
		*/
		void contract(size_t _nodeId1, size_t _nodeId2);
		
		double contraction_cost(size_t _nodeId1, size_t _nodeId2);
		
		
		/**
		* contracts the nodes with indices included in the set
		* replaces all but one node with degree-0 tensor
		* @returns the id of the contracted tensor
		*/
		size_t contract(std::set<size_t> _ids);
		
		virtual value_t frob_norm() const;
		
		/**
		 * checks whether the given TensorNetwork adheres to the format definition it expects according to its other virtual overloads
		 * eg. calls is_valid_tt for tt tensors and is_valid_network by default
		 */
		virtual bool is_in_expected_format() const;
    };
}