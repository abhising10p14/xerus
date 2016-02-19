// Xerus - A General Purpose Tensor Library
// Copyright (C) 2014-2016 Benjamin Huber and Sebastian Wolf. 
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
* @brief Implementation of the suitesparse wrapper functions.
*/

#include <xerus/cholmod_wrapper.h>
#include <xerus/index.h>
#include <xerus/tensor.h>
#include <xerus/misc/performanceAnalysis.h>
#include <xerus/misc/basicArraySupport.h>

#include <suitesparse/umfpack.h>

#include <xerus/misc/check.h>

namespace xerus { namespace internal {
	
	CholmodCommon::RestrictedAccess::RestrictedAccess(cholmod_common* const _c, std::mutex& _lock) 
		: c(_c), lock(_lock)
	{
		lock.lock();
	}
	
	CholmodCommon::RestrictedAccess::operator cholmod_common*() const {
		return c;
	}
	
	CholmodCommon::RestrictedAccess::~RestrictedAccess() {
		lock.unlock();
	}

	static void error_handler(int status, const char *file, int line, const char *message) {
		if (status < 0) {
			LOG(fatal, "CHOLMOD had a fatal error in " << file << ":" << line << " (status: " << status << ") msg: " << message);
		} else {
			LOG(cholmod_warning, "CHOLMOD warns in " << file << ":" << line << " (status: " << status << ") msg: " << message);
		}
	}
	
	CholmodCommon::CholmodCommon() : c(new cholmod_common()) {
		cholmod_start(c.get());
		REQUIRE(c->itype == CHOLMOD_INT, "atm only cholmod compiled with itype = int is supported...");
		REQUIRE(c->dtype == CHOLMOD_DOUBLE, "atm only cholmod compiled with dtype = double is supported...");
		c->error_handler = &error_handler;
		c->print = 0;
		REQUIRE(c->status == 0, "unable to initialize CHOLMOD");
	}
	
	CholmodCommon::~CholmodCommon() {
		cholmod_finish(c.get());
	}

	CholmodCommon::RestrictedAccess CholmodCommon::get() {
		return RestrictedAccess(c.get(), lock);
	}

	std::function<void(cholmod_sparse*)> CholmodCommon::get_deleter() {
		return [&](cholmod_sparse* _toDelete) {
			cholmod_free_sparse(&_toDelete, this->get());
		};
	}
	
	thread_local CholmodCommon cholmodObject;

	
	
	CholmodSparse::CholmodSparse(cholmod_sparse* _matrix) : matrix(_matrix, cholmodObject.get_deleter()) { }

	
	CholmodSparse::CholmodSparse(const size_t _m, const size_t _n, const size_t _N) 
		 : matrix(cholmod_allocate_sparse(_m, _n, _N, 1, 1, 0, CHOLMOD_REAL, cholmodObject.get()), cholmodObject.get_deleter())
	{
		REQUIRE(matrix && cholmodObject.c->status == 0, "cholmod_allocate_sparse did not allocate anything... status: " << cholmodObject.c->status << " call: " << _m << " " << _n << " " << _N << " alloc: " << cholmodObject.c->malloc_count);
	}

	CholmodSparse::CholmodSparse(const std::map<size_t, double>& _input, const size_t _m, const size_t _n, const bool _transpose) 
		: CholmodSparse(_n, _m, _input.size())
	{
		size_t entryPos = 0;
		int* i = static_cast<int*>(matrix->i);
		int* p = static_cast<int*>(matrix->p);
		double* x = static_cast<double*>(matrix->x);
		i[0] = 0;
		
		// create compressed column storage of A^T aka compressed row storage of A
		
		int currRow = -1;
		
		for(const std::pair<size_t, value_t>& entry : _input) {
			x[entryPos] = entry.second;
			i[entryPos] = static_cast<int>(entry.first%_n);
			while(currRow < static_cast<int>(entry.first/_n)) {
				p[++currRow] = int(entryPos);
			}
			entryPos++;
		}
		
		REQUIRE(size_t(currRow) < _m && entryPos == _input.size(), "Internal Error " << currRow << ", " << _m << " | " << entryPos << ", " <<  _input.size());
		
		while(currRow < static_cast<int>(_m)) {
			p[++currRow] = int(entryPos);
		}

		if(!_transpose) {
			// we didn't want A^T, so transpose the data to get compressed column storage of A
			ptr_type newM(cholmod_allocate_sparse(_m, _n, _input.size(), 1, 1, 0, CHOLMOD_REAL, cholmodObject.get()), cholmodObject.get_deleter());
			cholmod_transpose_unsym(matrix.get(), 1, nullptr, nullptr, 0, newM.get(), cholmodObject.get());
			matrix = std::move(newM);
		}
	}

	std::map<size_t, double> CholmodSparse::to_map(double _alpha) const {
		std::map<size_t, double> result;
		int* mi = reinterpret_cast<int*>(matrix->i);
		int* p = reinterpret_cast<int*>(matrix->p);
		double* x = reinterpret_cast<double*>(matrix->x);
		
		for(size_t i = 0; i < matrix->ncol; ++i) {
			for(int j = p[i]; j < p[i+1]; ++j) {
				IF_CHECK( auto ret = ) result.emplace(size_t(mi[size_t(j)])*matrix->ncol+i, _alpha*x[j]);
				REQUIRE(ret.second, "Internal Error");
			}
		}
		return result;
	}

	CholmodSparse CholmodSparse::operator*(const CholmodSparse& _rhs) const {
		return CholmodSparse(cholmod_ssmult(matrix.get(), _rhs.matrix.get(), 0, 1, 1, cholmodObject.get()));
	}

	
	void CholmodSparse::matrix_matrix_product( std::map<size_t, double>& _C,
								const size_t _leftDim,
								const size_t _rightDim,
								const double _alpha,
								const std::map<size_t, double>& _A,
								const bool _transposeA,
								const size_t _midDim,
								const std::map<size_t, double>& _B,
								const bool _transposeB ) 
	{
// 		LOG(ssmult, _leftDim << " " << _midDim << " " << _rightDim << " " << _transposeA << " " << _transposeB);
		const CholmodSparse lhsCs(_A, _transposeA?_midDim:_leftDim, _transposeA?_leftDim:_midDim, _transposeA);
		const CholmodSparse rhsCs(_B, _transposeB?_rightDim:_midDim, _transposeB?_midDim:_rightDim, _transposeB);
		const CholmodSparse resultCs = lhsCs * rhsCs;
		_C = resultCs.to_map(_alpha);
	}
	
	void CholmodSparse::solve_sparse_rhs(std::map<size_t, double>& _x,
						  size_t _xDim,
					   const std::map<size_t, double>& _A,
					   const bool _transposeA,
					   const std::map<size_t, double>& _b,
					   size_t _bDim)
	{
		LOG(fatal, "not yet implemented");
	}
	
	void CholmodSparse::solve_dense_rhs(double * _x,
								 size_t _xDim,
							  const std::map<size_t, double>& _A,
							  const bool _transposeA,
							  const double* _b,
							  size_t _bDim)
	{
		REQUIRE(_xDim == _bDim, "solving sparse systems only implemented for square matrices so far");
		const CholmodSparse A(_A, _transposeA?_xDim:_bDim, _transposeA?_bDim:_xDim, _transposeA);
		
		void *symbolic, *numeric;
// 		double control[UMFPACK_CONTROL], info[UMFPACK_INFO];
		// TODO check return values
		umfpack_di_symbolic(int(_bDim), int(_xDim), static_cast<int*>(A.matrix->p), static_cast<int*>(A.matrix->i), static_cast<double*>(A.matrix->x), &symbolic, nullptr, nullptr);
		umfpack_di_numeric(static_cast<int*>(A.matrix->p), static_cast<int*>(A.matrix->i), static_cast<double*>(A.matrix->x), symbolic, &numeric, nullptr, nullptr);
		umfpack_di_free_symbolic(&symbolic);
		umfpack_di_solve(UMFPACK_A, static_cast<int*>(A.matrix->p), static_cast<int*>(A.matrix->i), static_cast<double*>(A.matrix->x), _x, _b, numeric, nullptr, nullptr);
		umfpack_di_free_numeric(&numeric);
	}

}}
