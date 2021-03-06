/*******************************************************************************
* ASTex:                                                                       *
* Copyright (C) IGG Group, ICube, University of Strasbourg, France             *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: https://astex-icube.github.io                                      *
* Contact information: astex@icube.unistra.fr                                  *
*                                                                              *
*******************************************************************************/



#include <ASTex/image_rgb.h>
#include <ASTex/image_gray.h>


namespace ASTex
{
/**
 *
 *  DIR: 0=Horizontal
 *       1=Vertical
 */
template<typename IMG, int DIR>
class MinCutBuffer
{
	using PIX = typename IMG::PixelType;
	using T = typename IMG::DataType;

	double* data_err_;
	double* data_err_cum_;
	int length_;
	int overlay_;

	const IMG& imA_;
	const IMG& imB_;

	std::vector<int> minPos_;

	std::function<double(const IMG&, const Index&, const IMG&, const Index&)> error_func_;

public:
	/**
	 * @brief constructor
	 * @param imA input image 1
	 * @param imB input image 2
	 * @param tw tile width
	 * @param to tile overlay
	 */
	MinCutBuffer(const IMG& imA, const IMG& imB, int tw, int to):
		imA_(imA), imB_(imB),
		length_(tw),overlay_(to),
		minPos_(tw)
	{
		data_err_ = new double[2*length_*overlay_];
		data_err_cum_ = data_err_+ length_*overlay_;
	}

	~MinCutBuffer()
	{
		delete[] data_err_;
	}

	template <typename ERROR_PIX>
	void set_error_func(const ERROR_PIX& ef)
	{
		error_func_ = ef;
	}



protected:

	inline double& error_local(int i, int j)
	{
		return data_err_[i+j*overlay_];
	}

	inline double&  error_cumul(int i, int j)
	{
		return data_err_cum_[i+j*overlay_];
	}

	/**
	 * @brief get minimum of a column or row
	 * @param c indice of column or row
	 * @return
	 */
	int minOf(int c)
	{
		double m = std::numeric_limits<double>::max();
		int mi=0;
		for (int i=0;i<overlay_;++i)
		{
			double e = error_cumul(i,c);
			if (e < m)
			{
				m = e;
				mi = i;
			}
		}
		return mi;
	}

	/**
	 * @brief get minimum Of 3 value (i,j) / (i-1,j) / (i+1,j)
	 * @param i col (row)
	 * @param j row (col)
	 * @return
	 */
	int minOf3(int i, int j)
	{
		double m = error_cumul(i,j);
		int mi = i;
		if (i>0)
		{
			double v = error_cumul(i-1,j);
			if (v<m)
			{
				m = v;
				mi = i-1;
			}
		}
		i++;
		if (i<overlay_)
		{
			double v = error_cumul(i,j);
			if (v<m)
			{
				mi = i;
			}
		}
		return mi;
	}


	/**
	 * @brief compute the path of cut
	 * @param posA position in A
	 * @param posB position in B
	 */
	void pathCut(const Index& posA, const Index& posB)
	{
//		using PIX = typename IMG::PixelType;
		// compute initial error
		auto dir_index = [] (int i,int j) {if (DIR==1) return gen_index(i,j); else return gen_index(j,i);};


		// compute initial error

		for (int j=0; j<length_; ++j)
			for (int i=0; i<overlay_; ++i)
			{
				Index inc = dir_index(i,j);
				error_local(i,j) = error_func_(imA_,posA+inc ,imB_,posB+inc);
			}

		// compute cumulative error

		// first row
		for(int i=0;i<length_;++i)
			error_cumul(i,0) = error_local(i,0);

		// others rows
		for(int j=1;j<overlay_;++j)
		{
			error_cumul(0,j) = error_local(0,j) + std::min(error_cumul(0,j-1),error_cumul(1,j-1));
			int i=1;
			while (i<length_-1)
			{
				error_cumul(i,j) = error_local(i,j) + std::min( std::min(error_cumul(i-1,j-1),error_cumul(i,j-1)), error_cumul(i+1,j-1)) ;
				++i;
			}
			error_cumul(i,j) = error_local(i,j) + std::min(error_cumul(i-1,j-1),error_cumul(i,j-1));
		}

		// up to store local position of min error cut
		int i=overlay_-1;

		minPos_[i] = minOf(i);
		while (i>0)
		{
			minPos_[i-1] = minOf3(minPos_[i],i-1);
			--i;
		}
	}

	void fusion(const Index& posA, const Index& posB, IMG& dst, const Index& pos)
	{
		pathCut(posA, posB);

		auto dir_index = [] (int i,int j) {if (DIR==1) return gen_index(i,j); return gen_index(j,i);};

		int y = pos[DIR];
		for(int i=0; i<length_;++i)
		{
			int x = pos[1-DIR];
			int j=0;
			while(j<minPos_[i])
			{
				dst.pixel_absolute(dir_index(x,y)) = imA_.pixel_absolute(dir_index(x,y));
				++x;
				++j;
			}
			dst.pixel_absolute(dir_index(x,y)) = blend(imA_.pixel_absolute(dir_index(x,y)),imB_.pixel_absolute(dir_index(x,y)),0.5);
			j++;
			while(j<overlay_)
			{
				dst.pixel_absolute(dir_index(x,y)) = imB_.pixel_absolute(dir_index(x,y));
				++x;
				++j;
			}
		}
	}

};



template <typename IMG>
inline double ssd_error_pixels(const IMG& imA, const Index& iA, const IMG& imB, const Index& iB)
{
	const typename IMG::PixelType& P = imA.pixelAbsolute(iA);
	const typename IMG::PixelType& Q = imB.pixelAbsolute(iB);

	double sum_err2 = 0.0;

	for (uint32_t i=0; i<IMG::NB_CHANNELS; ++i)
	{
		double err = IMG::normalized_value(IMG::channel(P,i)) - IMG::normalized_value(IMG::channel(Q,i));
		sum_err2 += err*err;
	}
	return sum_err2;
}


} // namespace ASTex
