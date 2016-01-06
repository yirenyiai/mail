#pragma once
#include <string>
#include <algorithm>

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/algorithm/string.hpp>



namespace boost
{
	namespace qp
	{
		template <class charset>
		std::basic_string<charset> qp_encode(const std::basic_string<charset>& input)
		{
			std::basic_string<charset> output;
			const charset hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

			for (auto it(input.begin()); it != input.end(); ++it)
			{
				const charset byte = *it;

				if ((byte == 0x20) || (byte >= 33) && (byte <= 126) && (byte != 61))
				{
					output.push_back(byte);
				}
				else
				{
					output.push_back('=');
					output.push_back(hex[((byte >> 4) & 0x0F)]);
					output.push_back(hex[(byte & 0x0F)]);
				}
			}

			return output;
		}


		template <class charset>
		std::basic_string<charset> qp_decode(const std::basic_string<charset>& input)
		{
			// 0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?  @  A   B   C   D   E   F
			const int hexVal[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15 };

			std::basic_string<charset> output;

			for (auto it(input.begin()); it != input.end(); ++it)
			{
				if (*it == '=')
				{
					output.push_back((hexVal[int(*(++it)) - '0'] << 4) + hexVal[int(*(++it)) - '0']);
				}
				else
				{
					output.push_back(int(*it));
				}
			}

			return output;
		}

	}

}