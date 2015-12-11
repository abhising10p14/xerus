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

/**
 * @file
 * @brief Implementation of the (indexed) Tensor evaluation (ie. generlized transpositions).
 */

#include <xerus/indexedTensor_tensor_operators.h>
#include <xerus/basic.h>
#include <xerus/index.h>
#include <xerus/misc/check.h>
#include <xerus/tensor.h>
 
#include <memory>
#include <xerus/selectedFunctions.h>
#include <xerus/misc/missingFunctions.h>
#include <xerus/misc/performanceAnalysis.h>

namespace xerus {

	/** 
	 * "increases" the imaginary indices that lead to the current pointer position of oldPosition to match the index i
	 * precondition: oldPosition corresponding to index (i-1)
	 * postcondition: oldPosition corresponding to index i
	 */
	void increase_indices(const size_t _i, const value_t*& _oldPosition, const size_t _numIndices, const size_t* const _steps, const size_t* const _multDimensions) {
		size_t index = _numIndices-1;
		_oldPosition += _steps[index];
		size_t multStep = _multDimensions[index];
		while(_i%multStep == 0) {
			_oldPosition -= _multDimensions[index]*_steps[index]; // "reset" current index to 0
			--index; // Advance to next index
			_oldPosition += _steps[index]; // increase next index
			multStep *= _multDimensions[index]; // next stepSize
		}
	}

	void sum_traces(   value_t* const _newPosition,
								const value_t* _oldPosition,
								const size_t* const _doubleSteps,
								const size_t* const _doubleMultDimensions,
								const size_t _numDoubleIndexPairs,
								const size_t _numSummations) {
		*_newPosition = *_oldPosition;
		for(size_t k = 1; k < _numSummations; ++k) {
			increase_indices(k, _oldPosition, _numDoubleIndexPairs, _doubleSteps, _doubleMultDimensions);
			*_newPosition += *_oldPosition;
		}
	}
	
	void sum_traces(   value_t* const _newPosition,
								const value_t* _oldPosition,
								const size_t* const _doubleSteps,
								const size_t* const _doubleMultDimensions,
								const size_t _numDoubleIndexPairs,
								const size_t _numSummations,
								const size_t _orderedIndicesMultDim ) {
		misc::array_copy(_newPosition, _oldPosition, _orderedIndicesMultDim);
		for(size_t k = 1; k < _numSummations; ++k) {
			increase_indices(k, _oldPosition, _numDoubleIndexPairs, _doubleSteps, _doubleMultDimensions);
			misc::array_add(_newPosition, 1.0, _oldPosition, _orderedIndicesMultDim);
		}
	}
	
	/**
	 * @brief: Performes the low level evaluation of a Tensor to another Tensor
	 * @param _outTensor: The tensor INTO which the evaluation is performed. The Dimensions must be set correctly
	 * @param _inputTensor: The tensor which is evalueted. 
	 * @param _fixedIndexOffset Offset in @a _inputTensor caused by fixed indices, e.g. 50 if the first index of an (7, 10) tensor is fixed to 5.
	 * @param _orderedBackDim: the combined dimension of all allready ordered indices at the back, e.g. of i and k in A(j,l,k,i) = B(l,j,k,i);
	 * @param _numOutIndicesToShuffle Number of indices in @a _outTensor minus the number of allready ordered indices in the back.
	 * @param _stepSizes The step of the indices, e.g. [21,3,1] for an (73, 7, 3) tensor.
	 * @param _outIndexDimensions Dimensions of the indices in @a _outTensor.
	 * @param _numTracedDimensions Number of trace PAIRS (e.g. 1 if there is one pair).
	 * @param _traceDimensions Dimension of each trace pair.
	 * @param _traceStepSizes step size of each trace pair, i.e. to added step size of the two indices correspodnign to the pair.
	 * @param _totalTraceDim basically product of _traceDimensions.
	 */
	void full_to_full_evaluation(Tensor& _outTensor,
								 const Tensor& _inputTensor,
								 const size_t _fixedIndexOffset,
								 const size_t _orderedBackDim,
								 const size_t _numOutIndicesToShuffle,
								 const size_t* const _stepSizes, 
								 const size_t* const _outIndexDimensions,
								 const size_t _numTracedDimensions,
								 const size_t* const _traceDimensions,
								 const size_t* const _traceStepSizes,
								 const size_t _totalTraceDim
								) {
		// Start performance analysis for low level part
		PA_START;
		
		// Get pointers to the data
		const value_t* oldPosition = _inputTensor.get_unsanitized_dense_data()+_fixedIndexOffset;
		value_t* const newPosition = _outTensor.get_unsanitized_dense_data();
		
		// Get specific sizes
		const size_t numSteps = _outTensor.size/_orderedBackDim;
		
		if(_orderedBackDim == 1) { // We have to copy/add single entries
			if(_totalTraceDim == 1) { // We don't need to sum any traces
				newPosition[0] = *oldPosition;
				for(size_t i = 1; i < numSteps; ++i) {
					increase_indices(i, oldPosition, _numOutIndicesToShuffle, _stepSizes, _outIndexDimensions);
					newPosition[i] = *oldPosition;
				}
			} else { // We have to add traces
				sum_traces(newPosition, oldPosition, _traceStepSizes, _traceDimensions, _numTracedDimensions, _totalTraceDim);
				for(size_t i = 1; i < numSteps; ++i) {
					increase_indices(i, oldPosition, _numOutIndicesToShuffle, _stepSizes, _outIndexDimensions);
					sum_traces(newPosition+i, oldPosition, _traceStepSizes, _traceDimensions, _numTracedDimensions, _totalTraceDim);
				}
			}
		} else { // We can copy/add larger blocks
			if(_totalTraceDim == 1) { // We don't need to sum any traces
				misc::array_copy(newPosition, oldPosition, _orderedBackDim);
				for(size_t i = 1; i < numSteps; ++i) {
					increase_indices(i, oldPosition, _numOutIndicesToShuffle, _stepSizes, _outIndexDimensions);
					misc::array_copy(newPosition + i*_orderedBackDim, oldPosition, _orderedBackDim);
				}
			} else { // We have to add traces
				sum_traces(newPosition, oldPosition, _traceStepSizes, _traceDimensions, _numTracedDimensions, _totalTraceDim, _orderedBackDim);
				for(size_t i = 1; i < numSteps; ++i) {
					increase_indices(i, oldPosition, _numOutIndicesToShuffle, _stepSizes, _outIndexDimensions);
					sum_traces(newPosition + i*_orderedBackDim, oldPosition, _traceStepSizes, _traceDimensions, _numTracedDimensions, _totalTraceDim, _orderedBackDim);
				}
			}
		}
		PA_END("Evaluation", "Full->Full", misc::to_string(_inputTensor.dimensions)+" ==> " + misc::to_string(_outTensor.dimensions));
	}
	
	
	size_t get_position(const std::pair<size_t, value_t>& _entry,
						const size_t* const _baseIndexDim,
						const size_t* const _divisors,
						const size_t* const _attributes,
						const size_t numIndices ) {
		size_t position = 0;
		for(size_t i = 0; i < numIndices; ++i) {
			position += ((_entry.first/_divisors[i])%_baseIndexDim[i])*_attributes[i];
		}
		return position;
	}

	bool check_position(size_t & _position,
						const std::pair<size_t, value_t>& _entry,
						const size_t* const _baseIndexDim,
						const size_t* const _divisors,
						const size_t* const _attributes,
						const bool* const _fixedFlags,
						const bool* const _traceFlags,
						const size_t numIndices ) {
		_position = 0;
		// Process each index
		for(size_t i = 0; i < numIndices; ++i) {
			const size_t indexPosition = (_entry.first/_divisors[i])%_baseIndexDim[i];
			
			// The position is only changed if the index is not fixed...
			if(_fixedFlags[i]) {
				if(indexPosition != _attributes[i]) {
					return false; // The indexPosition differs from the fixed position => Do not add this value
				}
			// ... and not part of a trace.
			} else if(_traceFlags[i]) {
				if(indexPosition != (_entry.first/_divisors[_attributes[i]])%_baseIndexDim[_attributes[i]]) {
					return false; // This entry is not on the diagonal of the trace => Do not add this value
				}
			} else {
				_position += indexPosition*_attributes[i];
			}
		}
		return true;
	}

	std::unique_ptr<const size_t[]> get_dimension_array(const std::vector<Index> _indices) {
		if(_indices.size() == 0) {
			return std::unique_ptr<const size_t[]>();
		} else {
			size_t* stepSizes = new size_t[_indices.size()];
			for(size_t i = 0; i < _indices.size(); ++i) {
				stepSizes[i] = _indices[i].dimension();
			}
			return std::unique_ptr<const size_t[]>(stepSizes);
		}
	}
	
	std::unique_ptr<const size_t[]> get_step_sizes(const std::vector<Index> _indices) {
		if(_indices.size() == 0) {
			return std::unique_ptr<const size_t[]>();
		} else {
			size_t* stepSizes = new size_t[_indices.size()];
			stepSizes[_indices.size()-1] = 1;
			for(size_t i = _indices.size()-1; i > 0; --i) {
				stepSizes[i-1] = stepSizes[i]*_indices[i].dimension();
			}
			return std::unique_ptr<const size_t[]>(stepSizes);
		}
	}


	void evaluate(IndexedTensorWritable<Tensor>&& _out, IndexedTensorReadOnly<Tensor>&& _base) {
		// Assign the indices
		_base.assign_indices();
		_out.assign_indices();
		
		// Extract base index dimensions
		const std::unique_ptr<const size_t[]> baseIndexDimensions = get_dimension_array(_base.indices);
		
		#ifndef DISABLE_RUNTIME_CHECKS_ // Performe complete check whether the input is valid
			REQUIRE(_out.tensorObjectReadOnly != _base.tensorObjectReadOnly, "Target of evaluation must not conincide with base!");
			REQUIRE(!_out.tensorObjectReadOnly->is_sparse() || _base.tensorObjectReadOnly->is_sparse(), "Evaluation of Tensor to SparseTensor not implemented and probably not useful.");
			
			// Check base indices
			for(size_t i = 0; i < _base.indices.size(); ++i) {
				// If the index is fixed we don't expect to find it anywhere
				if(_base.indices[i].fixed()) {
					REQUIRE(_base.indices[i].span == 1, "Fixed indices must have span one. Indices are: " << _base.indices << ", total should be " << _base.indices.size() << ". The problem is: " << _base.indices[i] << " -- " << _base.indices[i].fixed());
					continue;
				}
				
				// Try to find index in _out
				size_t j = 0;
				while(j < _out.indices.size() && _base.indices[i] != _out.indices[j]) { ++j; }
				if(j < _out.indices.size()) {
					REQUIRE(_base.indices[i].dimension() == _out.indices[j].dimension(), "The indexDimensions in the target and base of evaluation must coincide. Here " << _base.indices[i].dimension() << "!=" << _out.indices[j].dimension() << ". For index " << _base.indices[i] << " == " << _out.indices[j]);
					REQUIRE(_base.indices[i].span == _out.indices[j].span, "The indexSpans in the target and base of evaluation must coincide.");
					REQUIRE(_base.indices[i].open(), "Indices appearing in the target of evaluation must not be part of a trace nor be fixed. Base: " << _base.indices << " Out: " << _out.indices);
					continue;
				}
				
				// Try to find index a second time in base
				j = 0;
				while(j < _base.indices.size() && (i == j || _base.indices[i] != _base.indices[j])) { ++j; }
				REQUIRE(j < _base.indices.size(), "All indices of evalutation base must either be fixed, appear in the target or be part of a trace. Base: " << _base.indices << " Out: " << _out.indices);
				REQUIRE(misc::count(_base.indices, _base.indices[i]) == 2, "Indices must appear at most two times. Base: " << _base.indices << " Out: " << _out.indices);
				REQUIRE(_base.indices[i].dimension() == _base.indices[j].dimension(), "The indexDimensions of two traced indices must conince.");
				REQUIRE(_base.indices[i].span == 1 && _base.indices[j].span == 1, "The indexSpans of traced indices must be one (It is ambigious what a trace of span 2 indices is meant to be).");
			}
			
			// Check out indices
			for(size_t i = 0; i < _out.indices.size(); ++i) {
				REQUIRE(_out.indices[i].open(),  "Traces and fixed indices are not allowed in the target of evaluation. Base: " << _base.indices << " Out: " << _out.indices);
				REQUIRE(misc::count(_base.indices, _out.indices[i]) == 1, "Every index of the target must appear exactly once in the base of evaluation. Base: " << _base.indices << " Out: " << _out.indices);
			}
		#endif
		
		// If there is no index reshuffling, the evalutation is trivial
		if(_base.indices == _out.indices) {
			*_out.tensorObject = *_base.tensorObjectReadOnly;
			return; // We are finished here
		}
		
		// We need the step sizes of the base indices
		const std::unique_ptr<const size_t[]> baseIndexStepSizes = get_step_sizes(_base.indices);
		
		
		// In every case we have to ensure that _out has its own data, since we gonna rewrite it.
		_out.tensorObject->ensure_own_data_no_copy();
		
		//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - Full => Full   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		if(!_out.tensorObjectReadOnly->is_sparse() && !_base.tensorObjectReadOnly->is_sparse()) {
			// Extract out index dimensions
			const std::unique_ptr<const size_t[]> outIndexDimensions = get_dimension_array(_out.indices);
		
			// Propagate the constant factor, since we won't apply it for Tensors
			_out.tensorObject->factor = _base.tensorObjectReadOnly->factor;
			
			// Count how many indices in the back are already ordered (We know that base has at least as many indices as out)
			size_t numOrderedIndices;
			for(numOrderedIndices = 0; numOrderedIndices < _out.indices.size() && _base.indices[_base.indices.size()-1-numOrderedIndices] == _out.indices[_out.indices.size()-1-numOrderedIndices]; ++numOrderedIndices) { }
			
			const size_t orderedIndexDim = baseIndexStepSizes[_base.indices.size()-numOrderedIndices-1];
			
			VLA(size_t[_out.indices.size()-numOrderedIndices], stepSizes); // How much we have to move in base when the i-th index of _out increases
			
			size_t fixedIndexOffset = 0; // Fixed offset in base due to fixed indices
			
			std::vector<size_t> traceStepSizes; // How much we have to move in base if the index of the i-th trace is increased
			std::vector<size_t> traceDimensions; // The dimensions of the traces
			size_t totalTraceDim = 1; // Total number of summantions, i.e. Product of all trace dimensions
			
			// Calculate stepSizes for our tensor. We will march in steps of orderedIndicesMultDim in _out.data
			for(size_t i = 0; i < _base.indices.size()-numOrderedIndices; ++i) {
				// First try to find the index in _out (if it is not contained it must be fixed or part of a trace)
				size_t outPos = 0;
				while(outPos < _out.indices.size() && _base.indices[i] != _out.indices[outPos]) { ++outPos; }
				
				if(outPos < _out.indices.size()) { // If we found it we are basically finished
					stepSizes[outPos] = baseIndexStepSizes[i];
				} else if(_base.indices[i].fixed()) { // One reason for an index to not be in _out is to be fixed
					fixedIndexOffset += size_t(_base.indices[i].valueId)*baseIndexStepSizes[i];
				} else { // If the Index is not fixed, then it has to be part of a trace
					for(size_t j = i+1; j < _base.indices.size()-numOrderedIndices; ++j) {
						if(_base.indices[i] == _base.indices[j]) {
							traceStepSizes.emplace_back(baseIndexStepSizes[i]+baseIndexStepSizes[j]);
							traceDimensions.emplace_back(_base.indices[i].dimension());
							totalTraceDim *= _base.indices[i].dimension();
							break;
						}
					}
				}
			}
			
			full_to_full_evaluation(*_out.tensorObject,
				*_base.tensorObjectReadOnly,
				fixedIndexOffset,
				orderedIndexDim,
				_out.indices.size()-numOrderedIndices,
				stepSizes,
				outIndexDimensions.get(),
				traceDimensions.size(),
				traceDimensions.data(), 
				traceStepSizes.data(),
				totalTraceDim
			);
		}
		//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - Sparse => Both  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		else if(_base.tensorObjectReadOnly->is_sparse()) {
			VLA(bool[_base.indices.size()]  , fixedFlags); // Flag for each index indicating whether the index is fixed
			VLA(bool[_base.indices.size()]  , traceFlags); // Flag for each index indicating whether the index is part of a trace
			VLA(size_t[_base.indices.size()], attributes);  // Either the factor in _out, the value of an fixed index or the position of the other part of a trace
			bool peacefullIndices = true;
			
			// Process base indices
			const std::unique_ptr<const size_t[]> outIndexStepSizes = get_step_sizes(_out.indices);
			for(size_t i = 0; i < _base.indices.size(); ++i) {
				// First try to find the index in out
				size_t outPos = 0;
				while(outPos < _out.indices.size() && _out.indices[outPos] != _base.indices[i]) { ++outPos; }
				if(outPos < _out.indices.size()) {
					fixedFlags[i] = false;
					traceFlags[i] = false;
					attributes[i] = outIndexStepSizes[outPos];
					continue;
				}
				
				// Check whether the index is fixed
				if(_base.indices[i].fixed()) {
					fixedFlags[i] = true;
					traceFlags[i] = false;
					attributes[i] = (size_t) _base.indices[i].valueId;
					peacefullIndices = false;
					continue;
				}
				
				// If the index is not in out and not fixed then it has to be part of a trace
				fixedFlags[i] = false;
				traceFlags[i] = true;
				peacefullIndices = false;
				for(attributes[i] = 0; _base.indices[i] != _base.indices[attributes[i]] || attributes[i] == i; ++attributes[i]) { }
			}
			
			// Get the entries and the factor of our base tensor
			const std::map<size_t, value_t>& baseEntries = _base.tensorObjectReadOnly->get_unsanitized_sparse_data();
			const value_t factor = _base.tensorObjectReadOnly->factor;
			
			// Start performance analysis for low level part
			PA_START;
			
			// Check whether _out is sparse
			if(_out.tensorObjectReadOnly->is_sparse()) {
				std::map<size_t, value_t>& outEntries = _out.tensorObject->override_sparse_data();
				
				if(peacefullIndices) {
					for(const std::pair<size_t, value_t>& entry : baseEntries) {
						outEntries.insert(std::pair<size_t, value_t>(get_position(entry, baseIndexDimensions.get(), baseIndexStepSizes.get(), attributes, _base.indices.size()), factor*entry.second));
					}
				} else {
					for(const std::pair<size_t, value_t>& entry : baseEntries) {
						size_t newPosition;
						if(check_position(newPosition, entry, baseIndexDimensions.get(), baseIndexStepSizes.get(), attributes, fixedFlags, traceFlags, _base.indices.size())) {
							outEntries[newPosition] += factor*entry.second;
						}
					}
				}
				PA_END("Evaluation", "Sparse->Sparse", misc::to_string(_base.tensorObjectReadOnly->dimensions)+" ==> " + misc::to_string(_out.tensorObjectReadOnly->dimensions));
			} else {
				// Ensure that _out is empty
				value_t* const dataPointer = _out.tensorObject->override_dense_data();
				misc::array_set_zero(dataPointer, _out.tensorObject->size);
				
				if(peacefullIndices) {
					for(const std::pair<size_t, value_t>& entry : baseEntries) {
						dataPointer[get_position(entry, baseIndexDimensions.get(), baseIndexStepSizes.get(), attributes, _base.indices.size())] = factor*entry.second;
					}
				} else {
					for(const std::pair<size_t, value_t>& entry : baseEntries) {
						size_t newPosition;
						if(check_position(newPosition, entry, baseIndexDimensions.get(), baseIndexStepSizes.get(), attributes, fixedFlags, traceFlags, _base.indices.size())) {
							dataPointer[newPosition] += factor*entry.second;
						}
					}
				}
				PA_END("Evaluation", "Sparse->Full", misc::to_string(_base.tensorObjectReadOnly->dimensions)+" ==> " + misc::to_string(_out.tensorObjectReadOnly->dimensions));
			}
		}
	}
}
