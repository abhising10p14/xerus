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

// ([a-zA-Z0-9]*)\.data_difference_frob_norm\((\{[0-9, +]*\})\) ?< ?([0-9e-]*)
// \1.compare_to_data(\2, \3)

#pragma once
#include "../xerus.h"

UNIT_TEST(FullTensor, remove_slate,
    std::mt19937_64 rnd;
    rnd.seed(0X5EED);
    
    double n=0;
    FullTensor A({3,3}, [&](const std::vector<size_t> &){ n+=1; return n; } );
    
    A.remove_slate(0,1);
    TEST(A.compare_to_data({1,2,3,7,8,9}, 1e-14));
    A.resize_dimension(0,3,1);
    TEST(A.compare_to_data({1,2,3,0,0,0,7,8,9}, 1e-14));
    A.remove_slate(1,0);
    TEST(A.compare_to_data({2,3,0,0,8,9}, 1e-14));
    A.resize_dimension(1,3,1);
    TEST(A.compare_to_data({2,0,3,0,0,0,8,0,9}, 1e-14));
)

UNIT_TEST(FullTensor, dimension_reduction,
    FullTensor A({2,2,2});
    A[{0,0,0}] = 1;
    A[{0,0,1}] = 2;
    A[{0,1,0}] = 3;
    A[{0,1,1}] = 4;
    A[{1,0,0}] = 5;
    A[{1,0,1}] = 6;
    A[{1,1,0}] = 7;
    A[{1,1,1}] = 8;
    
    FullTensor B = A;
    FullTensor C;
    C = A;
    
    A.resize_dimension(0,1);
    TEST(A.compare_to_data({1,2,3,4}, 1e-13));
    TEST(A.dimensions[0] == 1);
    TEST(A.size == 4);
    
    B.resize_dimension(1,1);
    TEST(B.compare_to_data({1,2,5,6}, 1e-13));
    TEST(B.dimensions[1] == 1);
    TEST(B.size == 4);
    
    C.resize_dimension(2,1);
    TEST(C.compare_to_data({1,3,5,7}, 1e-13));
    TEST(C.dimensions[2] == 1);
    TEST(C.size == 4);
)

UNIT_TEST(FullTensor, dimension_expansion,
    FullTensor A({2,2,2});
    A[{0,0,0}] = 1;
    A[{0,0,1}] = 2;
    A[{0,1,0}] = 3;
    A[{0,1,1}] = 4;
    A[{1,0,0}] = 5;
    A[{1,0,1}] = 6;
    A[{1,1,0}] = 7;
    A[{1,1,1}] = 8;
    
    FullTensor B = A;
    FullTensor C;
    C = A;
    
    A.resize_dimension(0,3);
    TEST(A.compare_to_data({1,2,3,4,5,6,7,8,0,0,0,0}, 1e-13));
    TEST(A.dimensions[0] == 3);
    TEST(A.size == 12);
    
    B.resize_dimension(1,3);
    TEST(B.compare_to_data({1,2,3,4,0,0,5,6,7,8,0,0}, 1e-13));
    TEST(B.dimensions[1] == 3);
    TEST(B.size == 12);
    
    C.resize_dimension(2,3);
    TEST(C.compare_to_data({1,2,0,3,4,0,5,6,0,7,8,0}, 1e-13));
    TEST(C.dimensions[2] == 3);
    TEST(C.size == 12);
)

UNIT_TEST(FullTensor, modify_elements,
    FullTensor A({4,4});
    FullTensor B({4,4,7});
    FullTensor C({2,8});
    
    A[{0,0}] = 1;
    A[{0,1}] = 2;
    A[{0,2}] = 3;
    A[{0,3}] = 4;
    A[{1,0}] = 5;
    A[{1,1}] = 6;
    A[{1,2}] = 7;
    A[{1,3}] = 8;
    A[{2,0}] = 9;
    A[{2,1}] = 10;
    A[{2,2}] = 11;
    A[{2,3}] = 12;
    A[{3,0}] = 13;
    A[{3,1}] = 14;
    A[{3,2}] = 15;
    A[{3,3}] = 16;
    
    A.modify_diag_elements([](value_t& _entry){});
    TEST(A.compare_to_data({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}));
    
    A.modify_diag_elements([](value_t& _entry){_entry = 73.5*_entry;});
    TEST(A.compare_to_data({73.5*1,2,3,4,5,73.5*6,7,8,9,10,73.5*11,12,13,14,15,73.5*16}));
    
    A.modify_diag_elements([](value_t& _entry, const size_t _position){_entry = 73.5*_entry - (value_t)_position;});
    TEST(A.compare_to_data({73.5*73.5*1,2,3,4,5,73.5*73.5*6-1.0,7,8,9,10,73.5*73.5*11-2.0,12,13,14,15,73.5*73.5*16-3.0}));
    
    A.reinterpret_dimensions({2,8});
    
    A.modify_diag_elements([](value_t& _entry){_entry = 0;});
    TEST(A.compare_to_data({0,2,3,4,5,73.5*73.5*6-1.0,7,8,9,0,73.5*73.5*11-2.0,12,13,14,15,73.5*73.5*16-3.0}));
    
    FAILTEST(B.modify_diag_elements([](value_t& _entry){return _entry;}));
    FAILTEST(B.modify_diag_elements([](value_t& _entry, const size_t _position){return _entry;}));
)


