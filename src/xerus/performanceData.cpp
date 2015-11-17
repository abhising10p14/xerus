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
 * @brief Implementation of the PerformanceData class.
 */

#include <string>
#include <fstream>
#include <xerus/performanceData.h>
#include <xerus/misc/missingFunctions.h>

namespace xerus {
	
	void PerformanceData::add(const size_t _itrCount, const xerus::value_t _residual, const std::vector<size_t> _ranks, const size_t _flags) {
		if (active) {
			if (startTime == ~0ul) {
				start();
			}
			data.emplace_back(_itrCount, get_elapsed_time(), _residual, _ranks, _flags);
			
			if(printProgress) {
				LOG(PerformanceData, "Iteration " << std::setw(4) << std::setfill(' ') << _itrCount 
					<< " Time: " << std::right << std::setw(6) << std::setfill(' ') << std::fixed << std::setprecision(2) << double(data.back().elapsedTime)*1e-6 
					<< "s Residual: " <<  std::setw(11) << std::setfill(' ') << std::scientific << std::setprecision(6) << _residual << " Flags: " << _flags << " Ranks: " << _ranks);
			}
		}
	}
	
	void PerformanceData::add(const xerus::value_t _residual, const TensorNetwork::RankTuple _ranks, const size_t _flags) {
		if (active) {
			if (data.empty()) {
				add(0, _residual, _ranks, _flags);
			} else {
				add(data.back().iterationCount+1, _residual, _ranks, _flags);
			}
		}
	}

	void PerformanceData::dump_to_file(const std::string &_fileName) const {
		std::string header;
		header += "# ";
		header += additionalInformation;
		misc::replace(header, "\n", "\n# ");
		header += "\n# \n#itr \ttime[us] \tresidual \tflags \tranks...\n";
		std::ofstream out(_fileName);
		out << header;
		for (const DataPoint &d : data) {
			out << d.iterationCount << '\t' << d.elapsedTime << '\t' << d.residual << '\t' << d.flags;
			for (size_t r : d.ranks) {
				out << '\t' << r;
			}
			out << '\n';
		}
		out.close();
	}
	
	misc::LogHistogram PerformanceData::get_histogram(const xerus::value_t _base, bool _assumeConvergence) const {
		misc::LogHistogram hist(_base);
		std::vector<PerformanceData::DataPoint> convergenceData(data);
		if (_assumeConvergence) {
			value_t finalResidual = data.back().residual;
			convergenceData.pop_back();
			for (auto &p : convergenceData) {
				p.residual -= finalResidual;
			}
		}
		
		for (size_t i = 1; i<convergenceData.size(); ++i) {
			if (convergenceData[i].residual >= convergenceData[i-1].residual) {
				continue;
			}
			
			// assume x_2 = x_1 * 2^(-alpha * delta-t)
			value_t relativeChange = convergenceData[i].residual/convergenceData[i-1].residual;
			value_t exponent = log(relativeChange) / log(2);
			size_t delta_t = convergenceData[i].elapsedTime - convergenceData[i-1].elapsedTime;
			value_t rate = - exponent / value_t(delta_t);
			hist.add(rate, delta_t);
		}
		return hist;
	}

	PerformanceData NoPerfData(false, false);
}
