#include "imagewidget.h"

#include "../structures/colormap.h"
#include "../structures/image2d.h"

#include "../strategy/algorithms/thresholdconfig.h"
#include "../strategy/algorithms/thresholdtools.h"

#include "../util/aologger.h"

#include "plot/colorscale.h"
#include "plot/horizontalplotscale.h"
#include "plot/verticalplotscale.h"
#include "plot/title.h"

#include <iostream>
#include <fstream>

#include <boost/algorithm/string.hpp>

ImageWidget::ImageWidget() :
	_isInitialized(false),
	_initializedWidth(0),
	_initializedHeight(0),
	_showOriginalMask(true),
	_showAlternativeMask(true),
	_colorMap(BWMap),
	_image(),
	_highlighting(false),
	_startHorizontal(0.0),
	_endHorizontal(1.0),
	_startVertical(0.0),
	_endVertical(1.0),
	_segmentedImage(),
	_horiScale(0),
	_vertScale(0),
	_colorScale(0),
	_plotTitle(0),
	_scaleOption(NormalScale),
	_showXYAxes(true),
	_showColorScale(true),
	_showXAxisDescription(true),
	_showYAxisDescription(true),
	_showZAxisDescription(true),
	_showTitle(true),
	_max(1.0), _min(0.0),
	_range(Winsorized),
	_cairoFilter(Cairo::FILTER_NEAREST),
	_manualTitle(false),
	_manualXAxisDescription(false),
	_manualYAxisDescription(false),
	_manualZAxisDescription(false),
	_mouseIsIn(false)
{
	_highlightConfig = new ThresholdConfig();
	_highlightConfig->InitializeLengthsSingleSample();

	add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_RELEASE_MASK |
		   Gdk::BUTTON_PRESS_MASK | Gdk::LEAVE_NOTIFY_MASK);
	signal_motion_notify_event().connect(sigc::mem_fun(*this, &ImageWidget::onMotion));
	signal_leave_notify_event().connect(sigc::mem_fun(*this, &ImageWidget::onLeave));
	signal_button_release_event().connect(sigc::mem_fun(*this, &ImageWidget::onButtonReleased));
	signal_draw().connect(sigc::mem_fun(*this, &ImageWidget::onDraw) );
}

ImageWidget::~ImageWidget()
{
	Clear();
	delete _highlightConfig;
}

void ImageWidget::Clear()
{
  if(HasImage())
	{
		_originalMask.reset();
		_alternativeMask.reset();
		delete _highlightConfig;
		_highlightConfig = new ThresholdConfig();
		_highlightConfig->InitializeLengthsSingleSample();
		_segmentedImage.reset();
		_image.reset();
	}
	delete _horiScale;
	_horiScale = 0;
	delete _vertScale;
	_vertScale = 0;
	delete _colorScale;
	_colorScale = 0;
	delete _plotTitle;
	_plotTitle = 0;
	_isInitialized = false;
}

bool ImageWidget::onDraw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	if(get_width() == (int) _initializedWidth && get_height() == (int) _initializedHeight)
		redrawWithoutChanges(get_window()->create_cairo_context(), get_width(), get_height());
	else
		Update();
	return true;
}

void ImageWidget::ZoomFit()
{
	_startHorizontal = 0.0;
	_endHorizontal = 1.0;
	_startVertical = 0.0;
	_endVertical = 1.0;
	_onZoomChanged.emit();
}

void ImageWidget::ZoomIn()
{
	double distX = (_endHorizontal-_startHorizontal)*0.25;
	_startHorizontal += distX;
	_endHorizontal -= distX;
	double distY = (_endVertical-_startVertical)*0.25;
	_startVertical += distY;
	_endVertical -= distY;
	_onZoomChanged.emit();
}

void ImageWidget::ZoomInOn(size_t x, size_t y)
{
	double xr = double(x) / _image->Width(), yr = double(y) / _image->Height();
	double distX = (_endHorizontal-_startHorizontal)*0.25;
	_startHorizontal = xr - distX;
	_endHorizontal = xr + distX;
	if(_startHorizontal < 0.0)
	{
		_endHorizontal -= _startHorizontal;
		_startHorizontal = 0.0;
	}
	if(_endHorizontal > 1.0)
	{
		_startHorizontal -= _endHorizontal - 1.0;
		_endHorizontal = 1.0;
	}
	double distY = (_endVertical-_startVertical)*0.25;
	_startVertical = yr - distY;
	_endVertical = yr + distY;
	if(_startVertical < 0.0)
	{
		_endVertical -= _startVertical;
		_startVertical = 0.0;
	}
	if(_endVertical > 1.0)
	{
		_startVertical -= _endVertical - 1.0;
		_endVertical = 1.0;
	}
	_onZoomChanged.emit();
}

void ImageWidget::ZoomOut()
{
	if(!IsZoomedOut())
	{
		double distX = (_endHorizontal-_startHorizontal)*0.5;
		_startHorizontal -= distX;
		_endHorizontal += distX;
		if(_startHorizontal < 0.0) {
			_endHorizontal -= _startHorizontal;
			_startHorizontal = 0.0;
		}
		if(_endHorizontal > 1.0) {
			_startHorizontal -= _endHorizontal-1.0;
			_endHorizontal = 1.0;
		}
		if(_startHorizontal < 0.0) _startHorizontal = 0.0;
		
		double distY = (_endVertical-_startVertical)*0.5;
		_startVertical -= distY;
		_endVertical += distY;
		if(_startVertical < 0.0) {
			_endVertical -= _startVertical;
			_startVertical = 0.0;
		}
		if(_endVertical > 1.0) {
			_startVertical -= _endVertical-1.0;
			_endVertical = 1.0;
		}
		if(_startVertical < 0.0) _startVertical = 0.0;
		_onZoomChanged.emit();
	}
}

void ImageWidget::Update()
{
	Glib::RefPtr<Gdk::Window> window = get_window();
	if(window && get_width() > 0 && get_height() > 0)
	{
		if(HasImage())
		{
			update(window->create_cairo_context(), get_width(), get_height());
			window->invalidate(false);
		}
		else {
			redrawWithoutChanges(window->create_cairo_context(), get_width(), get_height());
		}
	}
}

void ImageWidget::SaveByExtension(const std::string& filename, unsigned width, unsigned height)
{
	const char* eMsg = "Saving image to file failed: could not determine file type from filename extension -- maybe the type is not supported. Supported types are .png, .svg or .pdf.";
	if(filename.size() < 4)
		throw std::runtime_error(eMsg);
	std::string ext = filename.substr(filename.size()-4);
	boost::to_lower<std::string>(ext);
	if(ext == ".png")
		SavePng(filename, width, height);
	else if(ext == ".svg")
		SaveSvg(filename, width, height);
	else if(ext == ".pdf")
		SavePdf(filename, width, height);
	else throw std::runtime_error(eMsg);
}

void ImageWidget::SavePdf(const std::string &filename, unsigned width, unsigned height)
{
	if(width == 0 || height == 0)
	{
		if(is_visible())
		{
			width = get_width();
			height = get_height();
		}
		else {
			width = 640;
			height = 480;
		}
	}
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create(filename, width, height);
	Cairo::RefPtr<Cairo::Context> cairo = Cairo::Context::create(surface);
	if(HasImage())
	{
		AOLogger::Debug << "Saving PDF of " << width << " x " << height << "\n";
		update(cairo, width, height);
	}
	cairo->show_page();
	// We finish the surface. This might be required, because some of the subclasses store the cairo context. In that
	// case, it won't be written.
	surface->finish();
}

void ImageWidget::SaveSvg(const std::string &filename, unsigned width, unsigned height)
{
	if(width == 0 || height == 0)
	{
		if(is_visible())
		{
			width = get_width();
			height = get_height();
		}
		else {
			width = 640;
			height = 480;
		}
	}
	Cairo::RefPtr<Cairo::SvgSurface> surface = Cairo::SvgSurface::create(filename, width, height);
	Cairo::RefPtr<Cairo::Context> cairo = Cairo::Context::create(surface);
	if(HasImage())
	{
		AOLogger::Debug << "Saving SVG of " << width << " x " << height << "\n";
		update(cairo, width, height);
	}
	cairo->show_page();
	surface->finish();
}

void ImageWidget::SavePng(const std::string &filename, unsigned width, unsigned height)
{
	if(width == 0 || height == 0)
	{
		if(is_visible())
		{
			width = get_width();
			height = get_height();
		}
		else {
			width = 640;
			height = 480;
		}
	}
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width, height);
	Cairo::RefPtr<Cairo::Context> cairo = Cairo::Context::create(surface);
	if(HasImage())
	{
		AOLogger::Debug << "Saving PNG of " << width << " x " << height << "\n";
		update(cairo, width, height);
	}
	surface->write_to_png(filename);
}

void ImageWidget::SaveText(const std::string &filename)
{
	if(HasImage())
	{
		Image2DCPtr image = _image;
		unsigned int
			startX = (unsigned int) round(_startHorizontal * image->Width()),
			startY = (unsigned int) round(_startVertical * image->Height()),
			endX = (unsigned int) round(_endHorizontal * image->Width()),
			endY = (unsigned int) round(_endVertical * image->Height());
		size_t
			imageWidth = endX - startX,
			imageHeight = endY - startY;
		AOLogger::Debug << "Saving text file for " << imageWidth << " x " << imageHeight << " values.\n";
		std::ofstream file(filename.c_str());
		file << imageWidth << '\n' << imageHeight << '\n';
		for(size_t y=startY; y!=endY; ++y)
		{
			for(size_t x=startX; x!=endX; ++x)
			{
				file << image->Value(x, y) << '\n';
			}
		}
	}
}

void ImageWidget::update(Cairo::RefPtr<Cairo::Context> cairo, unsigned width, unsigned height)
{
	Image2DCPtr image = _image;
	Mask2DCPtr mask = GetActiveMask(), originalMask = _originalMask, alternativeMask = _alternativeMask;
	
	unsigned int
		startX = (unsigned int) round(_startHorizontal * image->Width()),
		startY = (unsigned int) round(_startVertical * image->Height()),
		endX = (unsigned int) round(_endHorizontal * image->Width()),
		endY = (unsigned int) round(_endVertical * image->Height()),
		startTimestep = startX,
		endTimestep = endX;
	size_t
		imageWidth = endX - startX,
		imageHeight = endY - startY;
		
	if(imageWidth > 30000)
	{
		int shrinkFactor = (imageWidth + 29999) / 30000;
		image = image->ShrinkHorizontally(shrinkFactor);
		mask = mask->ShrinkHorizontally(shrinkFactor);
		if(originalMask != 0)
			originalMask = originalMask->ShrinkHorizontally(shrinkFactor);
		if(alternativeMask != 0)
			alternativeMask = alternativeMask->ShrinkHorizontally(shrinkFactor);
		startX /= shrinkFactor;
		endX /= shrinkFactor;
		imageWidth = endX - startX;
	}

	num_t min, max;
	findMinMax(image, mask, min, max);
	
	// If these are not yet created, they are 0, so ok to delete.
	delete _horiScale;
	delete _vertScale;
	delete _colorScale;
	delete _plotTitle;
		
	if(_showXYAxes)
	{
		_vertScale = new VerticalPlotScale();
		_vertScale->SetDrawWithDescription(_showYAxisDescription);
		_horiScale = new HorizontalPlotScale();
		_horiScale->SetDrawWithDescription(_showXAxisDescription);
	} else {
		_vertScale = 0;
		_horiScale = 0;
	}
	if(_showColorScale)
	{
		_colorScale = new ColorScale();
		_colorScale->SetDrawWithDescription(_showZAxisDescription);
	} else {
		_colorScale = 0;
	}
	if(_showXYAxes)
	{
		if(_metaData != 0 && _metaData->HasBand()) {
			_vertScale->InitializeNumericTicks(_metaData->Band().channels[startY].frequencyHz / 1e6, _metaData->Band().channels[endY-1].frequencyHz / 1e6);
			_vertScale->SetUnitsCaption("Frequency (MHz)");
		} else {
			_vertScale->InitializeNumericTicks(-0.5 + startY, 0.5 + endY - 1.0);
		}
		if(_metaData != 0 && _metaData->HasObservationTimes())
		{
			_horiScale->InitializeTimeTicks(_metaData->ObservationTimes()[startTimestep], _metaData->ObservationTimes()[endTimestep-1]);
			_horiScale->SetUnitsCaption("Time (UTC, hh:mm:ss)");
		} else {
			_horiScale->InitializeNumericTicks(-0.5 + startTimestep, 0.5 + endTimestep - 1.0);
		}
		if(_manualXAxisDescription)
			_horiScale->SetUnitsCaption(_xAxisDescription);
		if(_manualYAxisDescription)
			_vertScale->SetUnitsCaption(_yAxisDescription);
	}
	if(_metaData != 0) {
		if(_showColorScale && _metaData->ValueDescription()!="")
		{
			if(_metaData->ValueUnits()!="")
				_colorScale->SetUnitsCaption(_metaData->ValueDescription() + " (" + _metaData->ValueUnits() + ")");
			else
				_colorScale->SetUnitsCaption(_metaData->ValueDescription());
		}
	}
	if(_showColorScale)
	{
		if(_scaleOption == LogScale)
			_colorScale->InitializeLogarithmicTicks(min, max);
		else
			_colorScale->InitializeNumericTicks(min, max);
		if(_manualZAxisDescription)
			_colorScale->SetUnitsCaption(_zAxisDescription);
	}

	if(_showTitle && !actualTitleText().empty())
	{
		_plotTitle = new Title();
		_plotTitle->SetText(actualTitleText());
		_plotTitle->SetPlotDimensions(width, height, 0.0);
		_topBorderSize = _plotTitle->GetHeight(cairo);
	} else {
		_plotTitle = 0;
		_topBorderSize = 10.0;
	}
	// The scale dimensions are depending on each other. However, since the height of the horizontal scale is practically
	// not dependent on other dimensions, we give the horizontal scale temporary width/height, so that we can calculate its height:
	if(_showXYAxes)
	{
		_horiScale->SetPlotDimensions(width, height, 0.0, 0.0);
		_bottomBorderSize = _horiScale->GetHeight(cairo);
		_rightBorderSize = _horiScale->GetRightMargin(cairo);
	
		_vertScale->SetPlotDimensions(width - _rightBorderSize + 5.0, height - _topBorderSize - _bottomBorderSize, _topBorderSize);
		_leftBorderSize = _vertScale->GetWidth(cairo);
	} else {
		_bottomBorderSize = 0.0;
		_rightBorderSize = 0.0;
		_leftBorderSize = 0.0;
	}
	if(_showColorScale)
	{
		_colorScale->SetPlotDimensions(width - _rightBorderSize, height - _topBorderSize, _topBorderSize);
		_rightBorderSize += _colorScale->GetWidth(cairo) + 5.0;
	}
	if(_showXYAxes)
	{
		_horiScale->SetPlotDimensions(width - _rightBorderSize + 5.0, height -_topBorderSize - _bottomBorderSize, _topBorderSize, 	_vertScale->GetWidth(cairo));
	}

	class ColorMap *colorMap = createColorMap();
	
	const double
		minLog10 = min>0.0 ? log10(min) : 0.0,
		maxLog10 = max>0.0 ? log10(max) : 0.0;
	if(_showColorScale)
	{
		for(unsigned x=0;x<256;++x)
		{
			num_t colorVal = (2.0 / 256.0) * x - 1.0;
			num_t imageVal;
			if(_scaleOption == LogScale)
				imageVal = exp10((x / 256.0) * (log10(max) - minLog10) + minLog10);
			else 
				imageVal = (max-min) * x / 256.0 + min;
			double
				r = colorMap->ValueToColorR(colorVal),
				g = colorMap->ValueToColorG(colorVal),
				b = colorMap->ValueToColorB(colorVal);
			_colorScale->SetColorValue(imageVal, r/255.0, g/255.0, b/255.0);
		}
	}
	
	_imageSurface.clear();
	_imageSurface =
		Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, imageWidth, imageHeight);

	_imageSurface->flush();
	unsigned char *data = _imageSurface->get_data();
	size_t rowStride = _imageSurface->get_stride();

	Mask2DPtr highlightMask;
	if(_highlighting)
	{
		highlightMask = Mask2D::CreateSetMaskPtr<false>(image->Width(), image->Height());
		_highlightConfig->Execute(image, highlightMask, true, 10.0);
	}
	const bool
		originalActive = _showOriginalMask && originalMask != 0,
		altActive = _showAlternativeMask && alternativeMask != 0;
	int orMaskR=255, orMaskG=0, orMaskB=255;
	int altMaskR=255, altMaskG=255, altMaskB=0;
	if(_colorMap == ViridisMap)
	{
		orMaskR=0; orMaskG=0; orMaskB=0;
		altMaskR=255; altMaskG=255; altMaskB=255;
	}
	for(unsigned long y=startY;y<endY;++y) {
		guint8* rowpointer = data + rowStride * (endY - y - 1);
		for(unsigned long x=startX;x<endX;++x) {
			int xa = (x-startX) * 4;
			unsigned char r,g,b,a;
			if(_highlighting && highlightMask->Value(x, y) != 0) {
				r = 255; g = 0; b = 0; a = 255;
			} else if(originalActive && originalMask->Value(x, y)) {
				r = orMaskR; g = orMaskG; b = orMaskB; a = 255;
			} else if(altActive && alternativeMask->Value(x, y)) {
				r = altMaskR; g = altMaskG; b = altMaskB; a = 255;
			} else {
				num_t val = image->Value(x, y);
				if(val > max) val = max;
				else if(val < min) val = min;

				if(_scaleOption == LogScale)
				{
					if(image->Value(x, y) <= 0.0)
						val = -1.0;
					else
						val = (log10(image->Value(x, y)) - minLog10) * 2.0 / (maxLog10 - minLog10) - 1.0;
				}
				else
					val = (image->Value(x, y) - min) * 2.0 / (max - min) - 1.0;
				if(val < -1.0) val = -1.0;
				else if(val > 1.0) val = 1.0;
				r = colorMap->ValueToColorR(val);
				g = colorMap->ValueToColorG(val);
				b = colorMap->ValueToColorB(val);
				a = colorMap->ValueToColorA(val);
			}
			rowpointer[xa]=b;
			rowpointer[xa+1]=g;
			rowpointer[xa+2]=r;
			rowpointer[xa+3]=a;
		}
	}
	delete colorMap;

	if(_segmentedImage != 0)
	{
		for(unsigned long y=startY;y<endY;++y) {
			guint8* rowpointer = data + rowStride * (y - startY);
			for(unsigned long x=startX;x<endX;++x) {
				if(_segmentedImage->Value(x,y) != 0)
				{
					int xa = (x-startX) * 4;
					rowpointer[xa]=IntMap::R(_segmentedImage->Value(x,y));
					rowpointer[xa+1]=IntMap::G(_segmentedImage->Value(x,y));
					rowpointer[xa+2]=IntMap::B(_segmentedImage->Value(x,y));
					rowpointer[xa+3]=IntMap::A(_segmentedImage->Value(x,y));
				}
			}
		}
	}
	_imageSurface->mark_dirty();

	while(_imageSurface->get_width() > (int) width || _imageSurface->get_height() > (int) height)
	{
		unsigned
			newWidth = _imageSurface->get_width(),
			newHeight = _imageSurface->get_height();
		if(newWidth > width)
			newWidth = width;
		if(newHeight > height)
			newHeight = height;
		downsampleImageBuffer(newWidth, newHeight);
	}

	_isInitialized = true;
	_initializedWidth = width;
	_initializedHeight = height;
	redrawWithoutChanges(cairo, width, height);
} 

ColorMap *ImageWidget::createColorMap()
{
	switch(_colorMap) {
		case BWMap:
			return new MonochromeMap();
		case InvertedMap:
			return new class InvertedMap();
		case HotColdMap:
			return new ColdHotMap();
		case RedBlueMap:
			return new class RedBlueMap();
		case RedYellowBlueMap:
			return new class RedYellowBlueMap();
		case FireMap:
			return new class FireMap();
		case BlackRedMap:
			return new class BlackRedMap();
		case ViridisMap:
			return new class ViridisMap();
		default:
			return 0;
	}
}

void ImageWidget::findMinMax(Image2DCPtr image, Mask2DCPtr mask, num_t &min, num_t &max)
{
	switch(_range)
	{
		case MinMax:
			max = ThresholdTools::MaxValue(image, mask);
			min = ThresholdTools::MinValue(image, mask);
		break;
		case Winsorized:
		{
			num_t mean, stddev, genMax, genMin;
			ThresholdTools::WinsorizedMeanAndStdDev(image, mask, mean, stddev);
			genMax = ThresholdTools::MaxValue(image, mask);
			genMin = ThresholdTools::MinValue(image, mask);
			max = mean + stddev*3.0;
			min = mean - stddev*3.0;
			if(genMin > min) min = genMin;
			if(genMax < max) max = genMax;
		}
		break;
		case Specified:
			min = _min;
			max = _max;
		break;
	}
	if(min == max)
	{
		min -= 1.0;
		max += 1.0;
	}
	if(_scaleOption == LogScale && min<=0.0)
	{
		if(max <= 0.0)
		{
			max = 1.0;
		}
		min = max / 10000.0;
	}
	if(_scaleOption == ZeroSymmetricScale)
	{
		if(fabs(max) > fabs(min))
		{
			max = fabs(max);
			min = -max;
		} else {
			min = -fabs(min);
			max = -min;
		}
	}
	_max = max;
	_min = min;
}

void ImageWidget::redrawWithoutChanges(Cairo::RefPtr<Cairo::Context> cairo, unsigned width, unsigned height)
{
	cairo->set_source_rgb(1.0, 1.0, 1.0);
	cairo->set_line_width(1.0);
	cairo->rectangle(0, 0, width, height);
	cairo->fill();
		
	if(_isInitialized) {
		int
			destWidth = width - (int) floor(_leftBorderSize + _rightBorderSize),
			destHeight = height - (int) floor(_topBorderSize + _bottomBorderSize),
			sourceWidth = _imageSurface->get_width(),
			sourceHeight = _imageSurface->get_height();
		cairo->save();
		cairo->translate((int) round(_leftBorderSize), (int) round(_topBorderSize));
		cairo->scale((double) destWidth / (double) sourceWidth, (double) destHeight / (double) sourceHeight);
		Cairo::RefPtr<Cairo::SurfacePattern> pattern = Cairo::SurfacePattern::create(_imageSurface);
		pattern->set_filter(_cairoFilter);
		cairo->set_source(pattern);
		cairo->rectangle(0, 0, sourceWidth, sourceHeight);
		cairo->clip();
		cairo->paint();
		cairo->restore();
		cairo->set_source_rgb(0.0, 0.0, 0.0);
		cairo->rectangle(round(_leftBorderSize), round(_topBorderSize), destWidth, destHeight);
		cairo->stroke();

		if(_showColorScale)
			_colorScale->Draw(cairo);
		if(_showXYAxes)
		{
			_vertScale->Draw(cairo);
			_horiScale->Draw(cairo);
		}
		if(_plotTitle != 0)
			_plotTitle->Draw(cairo);
	}
}

void ImageWidget::downsampleImageBuffer(unsigned newWidth, unsigned newHeight)
{
	_imageSurface->flush();
	const unsigned
		oldWidth = _imageSurface->get_width(),
		oldHeight = _imageSurface->get_height();
	
	Cairo::RefPtr<Cairo::ImageSurface> newImageSurface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, newWidth, newHeight);

	unsigned char* newData = newImageSurface->get_data();
	size_t rowStrideOfNew = newImageSurface->get_stride();

	unsigned char *oldData = _imageSurface->get_data();
	size_t rowStrideOfOld = _imageSurface->get_stride();

	for(unsigned int y=0;y<newHeight;++y) {
		guint8* rowpointerToNew = newData + rowStrideOfNew * y;
		
		for(unsigned int x=0;x<newWidth;++x) {
			unsigned int r=0, g=0, b=0, a=0;
			
			const unsigned
				xOldStart = x * oldWidth / newWidth,
				xOldEnd = (x+1) * oldWidth / newWidth,
				yOldStart = y * oldHeight / newHeight,
				yOldEnd = (y+1) * oldHeight / newHeight;
			
			for(unsigned int yOld=yOldStart;yOld<yOldEnd;++yOld)
			{
				unsigned char *rowpointerToOld = oldData + rowStrideOfOld * yOld + xOldStart*4;
				for(unsigned int xOld=xOldStart;xOld<xOldEnd;++xOld)
				{
					r += (*rowpointerToOld); ++rowpointerToOld;
					g += (*rowpointerToOld); ++rowpointerToOld;
					b += (*rowpointerToOld); ++rowpointerToOld;
					a += (*rowpointerToOld); ++rowpointerToOld;
				}
			}
			
			const unsigned count = (xOldEnd - xOldStart) * (yOldEnd - yOldStart);
			(*rowpointerToNew) = (unsigned char) (r/count);
			++rowpointerToNew;
			(*rowpointerToNew) = (unsigned char) (g/count);
			++rowpointerToNew;
			(*rowpointerToNew) = (unsigned char) (b/count);
			++rowpointerToNew;
			(*rowpointerToNew) = (unsigned char) (a/count);
			++rowpointerToNew;
		}
	}

	_imageSurface = newImageSurface;
	_imageSurface->mark_dirty();
}

Mask2DCPtr ImageWidget::GetActiveMask() const
{
	if(!HasImage())
		throw std::runtime_error("GetActiveMask() called without image");
	const bool
		originalActive = _showOriginalMask && _originalMask != 0,
		altActive = _showAlternativeMask && _alternativeMask != 0;
	if(originalActive)
	{
		if(altActive)
		{
			Mask2DPtr mask = Mask2D::CreateCopy(_originalMask); 
			mask->Join(_alternativeMask);
			return mask;
		} else
			return _originalMask;
	} else {
		if(altActive)
			return _alternativeMask;
		else
			return Mask2D::CreateSetMaskPtr<false>(_image->Width(), _image->Height());
	}
}

TimeFrequencyMetaDataCPtr ImageWidget::GetSelectedMetaData()
{
	TimeFrequencyMetaDataCPtr metaData = _metaData;

	if(_startVertical != 0 && metaData != 0)
	{
		size_t startChannel = round(StartVertical() * _image->Height());
		TimeFrequencyMetaData *newData = new TimeFrequencyMetaData(*metaData);
		metaData = TimeFrequencyMetaDataCPtr(newData);
		BandInfo band = newData->Band();
		band.channels.erase(band.channels.begin(), band.channels.begin()+startChannel );
		newData->SetBand(band);
	}
	if(_startHorizontal != 0 && metaData != 0)
	{
		size_t startTime = round(StartHorizontal() * _image->Width());
		TimeFrequencyMetaData *newData = new TimeFrequencyMetaData(*metaData);
		metaData = TimeFrequencyMetaDataCPtr(newData);
		std::vector<double> obsTimes = newData->ObservationTimes();
		obsTimes.erase(obsTimes.begin(), obsTimes.begin()+startTime );
		newData->SetObservationTimes(obsTimes);
	}
	
	return metaData;
}

bool ImageWidget::toUnits(double mouseX, double mouseY, int &posX, int &posY)
{
	const unsigned int
		startX = (unsigned int) round(_startHorizontal * _image->Width()),
		startY = (unsigned int) round(_startVertical * _image->Height()),
		endX = (unsigned int) round(_endHorizontal * _image->Width()),
		endY = (unsigned int) round(_endVertical * _image->Height());
	const unsigned
		width = endX - startX,
		height = endY - startY;
	posX = (int) round((mouseX - _leftBorderSize) * width / (get_width() - _rightBorderSize - _leftBorderSize) - 0.5);
	posY = (int) round((mouseY - _topBorderSize) * height / (get_height() - _bottomBorderSize - _topBorderSize) - 0.5);
	bool inDomain = posX >= 0 && posY >= 0 && posX < (int) width && posY < (int) height;
	posX += startX;
	posY = endY - posY - 1;
	return inDomain;
}

bool ImageWidget::onMotion(GdkEventMotion *event)
{
	if(HasImage())
	{
		int posX, posY;
		if(toUnits(event->x, event->y, posX, posY))
		{
			_mouseX = posX;
			_mouseY = posY;
			_mouseIsIn = true;
			_onMouseMoved(posX, posY);
		} else if(_mouseIsIn) {
			_onMouseLeft();
			_mouseIsIn = false;
		}
	}
	return true;
}

bool ImageWidget::onLeave(GdkEventCrossing *event)
{
	if(_mouseIsIn)
	{
		_onMouseLeft();
		_mouseIsIn = false;
	}
	return true;
}

bool ImageWidget::onButtonReleased(GdkEventButton *event)
{
	if(HasImage())
	{
		int posX, posY;
		if(toUnits(event->x, event->y, posX, posY))
			_onButtonReleased(posX, posY);
	}
	return true;
}
