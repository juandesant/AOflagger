#ifndef GUI_QUALITY__2DPLOTPAGE_H
#define GUI_QUALITY__2DPLOTPAGE_H

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/expander.h>
#include <gtkmm/frame.h>

#include "../../quality/qualitytablesformatter.h"

#include "../plot/plot2d.h"
#include "../plot/plotwidget.h"

#include <set>

/**
	@author A.R. Offringa <offringa@astro.rug.nl>
*/
class TwoDimensionalPlotPage : public Gtk::HBox {
	public:
		TwoDimensionalPlotPage();
    virtual ~TwoDimensionalPlotPage();

		void SetStatistics(class StatisticsCollection *statCollection, const std::vector<AntennaInfo> &antennas)
		{
			processStatistics(statCollection, antennas);
			
			_statCollection = statCollection;
			// We need to do this here because it can not be done yet during construction in the
			// constructor (virtual methods not yet available there).
			if(!_customButtonsCreated)
			{
				addCustomPlotButtons(_plotBox);
				_customButtonsCreated = true;
			}
			updatePlot();
		}
		void CloseStatistics()
		{
			_statCollection = 0;
		}
		bool HasStatistics() const
		{
			return _statCollection != 0;
		}
		void SavePdf(const std::string& filename, QualityTablesFormatter::StatisticKind kind);
	protected:
		virtual void processStatistics(class StatisticsCollection *, const std::vector<AntennaInfo> &)
		{
		}
		
		virtual const std::map<double, class DefaultStatistics> &GetStatistics() const = 0;
		
		virtual void StartLine(Plot2D &plot, const std::string &name, const std::string &yAxisDesc) = 0;
		
		virtual void processPlot(Plot2D &plot)
		{
		}
		
		virtual void addCustomPlotButtons(Gtk::VBox &container)
		{
		}
		
		class StatisticsCollection *GetStatCollection() const
		{
			return _statCollection;
		}
		void updatePlot();
		
		//unsigned selectedKindCount() const;
	private:
		enum PhaseType { AmplitudePhaseType, PhasePhaseType, RealPhaseType, ImaginaryPhaseType} ;
		
		void updatePlotForSettings(
			const std::set<QualityTablesFormatter::StatisticKind>& kinds,
			const std::set<std::pair<unsigned int, unsigned int> >& pols,
			const std::set<PhaseType>& phases
		);
		
		void updatePlotConfig();
		void updateDataWindow();
		
		inline double getValue(enum PhaseType Phase, const std::complex<long double> val);
		
		std::set<QualityTablesFormatter::StatisticKind> getSelectedKinds() const;
		std::set<std::pair<unsigned, unsigned> > getSelectedPolarizations() const;
		std::set<enum PhaseType> getSelectedPhases() const;
		
		void plotStatistic(QualityTablesFormatter::StatisticKind kind, unsigned polA, unsigned polB, PhaseType phase, const std::string& yDesc);
		
		void initStatisticKindButtons();
		void initPolarizationButtons();
		void initPhaseButtons();
		void initPlotButtons();
		
		void onLogarithmicClicked()
		{
			_zeroAxisButton.set_sensitive(!_logarithmicButton.get_active());
			updatePlotConfig();
		}
		void onPlotPropertiesClicked();
		void onDataExportClicked();
		
		Gtk::Expander _expander;
		Gtk::VBox _sideBox;
		
		Gtk::Frame _statisticFrame;
		Gtk::VBox _statisticBox;
		Gtk::CheckButton _countButton, _meanButton, _stdDevButton, _varianceButton, _dCountButton, _dMeanButton, _dStdDevButton,  _rfiPercentageButton, _snrButton;
		
		Gtk::Frame _polarizationFrame;
		Gtk::VBox _polarizationBox;
		Gtk::CheckButton _polXXButton, _polXYButton, _polYXButton, _polYYButton, _polXXandYYButton, _polXYandYXButton;
		
		Gtk::Frame _phaseFrame;
		Gtk::VBox _phaseBox;
		Gtk::CheckButton _amplitudeButton, _phaseButton, _realButton, _imaginaryButton;
		
		Gtk::Frame _plotFrame;
		Gtk::VBox _plotBox;
		Gtk::CheckButton _logarithmicButton, _zeroAxisButton;
		Gtk::Button _plotPropertiesButton, _dataExportButton;
		
		class StatisticsCollection *_statCollection;
		Plot2D _plot;
		PlotWidget _plotWidget;
		
		class PlotPropertiesWindow *_plotPropertiesWindow;
		class DataWindow *_dataWindow;
		
		bool _customButtonsCreated;
		
		std::string getYDesc(const std::set<QualityTablesFormatter::StatisticKind>& kinds) const;
};

#endif
