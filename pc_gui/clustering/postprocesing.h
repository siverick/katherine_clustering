
/**
 * @postprocessing.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
#include "cluster_definition.h"
#include <QtCharts>

class postprocesing
{
public:

	/// <summary>
	/// Plot all clusters into the given QPixmap ref.
	/// </summary>
	/// <param name="picture">QPixmap for plotting</param>
	/// <param name="painter">Corresponding QPainter</param>
	static void plot_whole_image(QPixmap& picture, QPainter& painter, std::vector<ClusterType>& doneClusters)
	{
		picture = QPixmap(256, 256);
		picture.fill(Qt::black);

		painter.begin(&picture);   // paint in picture
		QPen pen;
		QPoint drawPt;
		QColor col;
		painter.setPen(pen);

		for (const auto& cluster : doneClusters)
		{
			for (const auto& pixel : cluster.pix)
			{
				col = get_color(pixel.ToT);
				/* Draw result */
				pen.setColor(col);
				painter.setPen(pen);
				drawPt.setX(pixel.x);
				drawPt.setY(pixel.y);
				painter.drawPoint(drawPt);
			}
		}
		painter.end();
	}

	// Incremental plotting of image
	static void plot_more_image(QPixmap& picture, QPainter& painter, std::vector<ClusterType>& doneClusters)
	{
		painter.begin(&picture);   // paint in picture
		QPen pen;
		QPoint drawPt;
		QColor col;
		painter.setPen(pen);

		for (const auto& cluster : doneClusters)
		{
			for (const auto& pixel : cluster.pix)
			{
				col = get_color(pixel.ToT);
				/* Draw result */
				pen.setColor(col);
				painter.setPen(pen);
				drawPt.setX(pixel.x);
				drawPt.setY(pixel.y);
				painter.drawPoint(drawPt);
			}
		}
		painter.end();
	}

	// Plot whole image of pixel counts (rtg)
	static void plot_whole_image(QPixmap& picture, QPainter& painter, PixelCounts* pixelCountMatrix)
	{
		picture = QPixmap(256, 256);
		picture.fill(Qt::black);

		painter.begin(&picture);   // paint in picture
		QPen pen;
		QPoint drawPt;
		QColor col;
		painter.setPen(pen);

		float dynamicRange = pixelCountMatrix->maxCount - pixelCountMatrix->minCount;
		for (int x = 0; x < 256; x++)
		{
			for (int y = 0; y < 256; y++)
			{
				col = get_count_color_linear(pixelCountMatrix->counts[x][y], dynamicRange, pixelCountMatrix->minCount);	// Linear scale
				//col = get_count_color_exp(pixelCountMatrix->counts[x][y], dynamicRange, pixelCountMatrix->minCount);	// Exponential scale
				/* Draw result */
				pen.setColor(col);
				painter.setPen(pen);
				drawPt.setX(x);
				drawPt.setY(y);
				painter.drawPoint(drawPt);
			}
		}
		painter.end();
	}

	// Convert vector of energies to histogram
	static std::vector<uint16_t> convert_energy_to_histogram(std::vector<uint16_t> done_energies)
	{
		std::vector<uint16_t> energies;
		/* Create energies vector where x = energie and y = their count in Clusters */
		for (const auto& energy : done_energies)     // SUM up THIS cluster energy
		{
			// Increase vector size until its big enough
			while ((energies.size() <= energy)) energies.emplace_back(0);	

			energies[energy] += 1;
		}

		return energies;
	}

	/// <summary>
	/// Generate RGB color from ToT Value.
	/// </summary>
	static QColor get_color(int toT)
	{
		QColor col = QColor();

		// Check zero color - make it visible
		if (toT == 0)
		{
			col.setRgb(0, 0, 255);
			col.setAlpha(150);
			return col;
		}

		/* Jednoduchy zpusob */
		float factor = (float)toT / 20.0f;
		if (factor < 0) factor = -factor;
		if (factor > 1) factor = 1;
		col.setRgb(255, factor * 255, factor * 255);
		if (factor < 0.33f)   col.setAlpha(10 + (245 * factor * 3));

		return col;
	}

private:

	/// <summary>
	/// Generate linear RGB color in pixel counting mode.
	/// </summary>
	static QColor get_count_color_linear(float count, float dynamicRange, float minCount)
	{
		QColor col = QColor();
		int alpha = 255 - static_cast<int>(((count - minCount) * 255) / dynamicRange);	// Linear

		/* Jednoduchy zpusob */
		col.setRgb(255, 255, 255, alpha);

		return col;
	}

	/// <summary>
	/// Generate exponential RGB color in pixel counting mode.
	/// </summary>
	static QColor get_count_color_exp(float count, float dynamicRange, float minCount)
	{
		QColor col = QColor();
		int alpha = 255 - static_cast<int>(pow(255, ((count - minCount) / dynamicRange)));	// Exponential

		/* Jednoduchy zpusob */
		col.setRgb(255, 255, 255, alpha);

		return col;
	}
};

