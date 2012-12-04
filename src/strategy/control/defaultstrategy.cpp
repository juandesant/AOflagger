#include "defaultstrategy.h"

#include "../actions/adapter.h"
#include "../actions/baselineselectionaction.h"
#include "../actions/calibratepassbandaction.h"
#include "../actions/changeresolutionaction.h"
#include "../actions/combineflagresultsaction.h"
#include "../actions/foreachbaselineaction.h"
#include "../actions/foreachcomplexcomponentaction.h"
#include "../actions/foreachmsaction.h"
#include "../actions/foreachpolarisationaction.h"
#include "../actions/frequencyselectionaction.h"
#include "../actions/highpassfilteraction.h"
#include "../actions/iterationaction.h"
#include "../actions/plotaction.h"
#include "../actions/setflaggingaction.h"
#include "../actions/setimageaction.h"
#include "../actions/slidingwindowfitaction.h"
#include "../actions/statisticalflagaction.h"
#include "../actions/strategyaction.h"
#include "../actions/sumthresholdaction.h"
#include "../actions/timeselectionaction.h"
#include "../actions/writeflagsaction.h"

namespace rfiStrategy {

	const unsigned
		DefaultStrategy::FLAG_NONE             = aoflagger::StrategyFlags::NONE,
		DefaultStrategy::FLAG_LOW_FREQUENCY    = aoflagger::StrategyFlags::LOW_FREQUENCY,
		DefaultStrategy::FLAG_HIGH_FREQUENCY   = aoflagger::StrategyFlags::HIGH_FREQUENCY,
		DefaultStrategy::FLAG_LARGE_BANDWIDTH  = aoflagger::StrategyFlags::LARGE_BANDWIDTH,
		DefaultStrategy::FLAG_SMALL_BANDWIDTH  = aoflagger::StrategyFlags::SMALL_BANDWIDTH,
		DefaultStrategy::FLAG_TRANSIENTS       = aoflagger::StrategyFlags::TRANSIENTS,
		DefaultStrategy::FLAG_ROBUST           = aoflagger::StrategyFlags::ROBUST,
		DefaultStrategy::FLAG_FAST             = aoflagger::StrategyFlags::FAST,
		DefaultStrategy::FLAG_OFF_AXIS_SOURCES = aoflagger::StrategyFlags::OFF_AXIS_SOURCES,
		DefaultStrategy::FLAG_UNSENSITIVE      = aoflagger::StrategyFlags::UNSENSITIVE,
		DefaultStrategy::FLAG_SENSITIVE        = aoflagger::StrategyFlags::SENSITIVE,
		DefaultStrategy::FLAG_GUI_FRIENDLY     = aoflagger::StrategyFlags::GUI_FRIENDLY,
		DefaultStrategy::FLAG_CLEAR_FLAGS      = aoflagger::StrategyFlags::CLEAR_FLAGS,
		DefaultStrategy::FLAG_AUTO_CORRELATION = aoflagger::StrategyFlags::AUTO_CORRELATION;
			
	Strategy *DefaultStrategy::CreateStrategy(enum TelescopeId telescopeId, unsigned flags, double frequency, double timeRes, double frequencyRes)
	{
		Strategy *strategy = new Strategy();
		LoadStrategy(*strategy, telescopeId, flags, frequency, timeRes, frequencyRes);
		return strategy;
	}
	
	void DefaultStrategy::LoadStrategy(ActionBlock &strategy, enum TelescopeId telescopeId, unsigned flags, double frequency, double timeRes, double frequencyRes)
	{
		bool calPassband =
			// Default MWA observations have strong frequency dependence
			(telescopeId==MWA_TELESCOPE && ((flags&FLAG_SMALL_BANDWIDTH) == 0)) ||
			// Other cases with large bandwidth
			((flags&FLAG_LARGE_BANDWIDTH) != 0);
		bool keepTransients = (flags&FLAG_TRANSIENTS) != 0;
		bool clearFlags =
			((flags&FLAG_CLEAR_FLAGS) != 0) ||
			((flags&FLAG_GUI_FRIENDLY) != 0);
		bool resetContaminated =
			((flags&FLAG_GUI_FRIENDLY) != 0);
		int iterationCount = ((flags&FLAG_ROBUST)==0) ? 2 : 4;
		
		LoadSingleStrategy(strategy, iterationCount, keepTransients, calPassband, clearFlags, resetContaminated);
	}
	
	void DefaultStrategy::LoadSingleStrategy(ActionBlock &block, int iterationCount, bool keepTransients, bool calPassband, bool clearFlags, bool resetContaminated)
	{
		ActionBlock *current;

		if(resetContaminated)
			block.Add(new SetImageAction());
		
		block.Add(new SetFlaggingAction());

		ForEachPolarisationBlock *fepBlock = new ForEachPolarisationBlock();
		block.Add(fepBlock);
		current = fepBlock;

		ForEachComplexComponentAction *focAction = new ForEachComplexComponentAction();
		focAction->SetOnAmplitude(true);
		focAction->SetOnImaginary(false);
		focAction->SetOnReal(false);
		focAction->SetOnPhase(false);
		focAction->SetRestoreFromAmplitude(false);
		current->Add(focAction);
		current = focAction;

		IterationBlock *iteration = new IterationBlock();
		iteration->SetIterationCount(iterationCount);
		iteration->SetSensitivityStart(2.0 * pow(2.0, iterationCount/2.0));
		current->Add(iteration);
		current = iteration;
		
		SumThresholdAction *t1 = new SumThresholdAction();
		t1->SetBaseSensitivity(1.0);
		if(keepTransients)
			t1->SetFrequencyDirectionFlagging(false);
		current->Add(t1);

		CombineFlagResults *cfr1 = new CombineFlagResults();
		current->Add(cfr1);

		cfr1->Add(new FrequencySelectionAction());
		if(!keepTransients)
			cfr1->Add(new TimeSelectionAction());
	
		current->Add(new SetImageAction());
		ChangeResolutionAction *changeResAction = new ChangeResolutionAction();
		if(keepTransients)
			changeResAction->SetTimeDecreaseFactor(1);
		else
			changeResAction->SetTimeDecreaseFactor(3);
		changeResAction->SetFrequencyDecreaseFactor(3);

		/*
		SlidingWindowFitAction *swfAction2 = new SlidingWindowFitAction();
		if(pulsar)
		{
			swfAction2->Parameters().timeDirectionWindowSize = 1;
		} else {
			swfAction2->Parameters().timeDirectionKernelSize = 2.5;
			swfAction2->Parameters().timeDirectionWindowSize = 10;
		}
		swfAction2->Parameters().frequencyDirectionKernelSize = 5.0;
		swfAction2->Parameters().frequencyDirectionWindowSize = 15;
		changeResAction2->Add(swfAction2);
		
		Replaced the sliding window fit action by the faster (SSE) high-pass
		filter on 26-08-2011
		*/
		HighPassFilterAction *hpAction = new HighPassFilterAction();
		if(keepTransients)
		{
			hpAction->SetWindowWidth(1);
		} else {
			hpAction->SetHKernelSigmaSq(2.5);
			hpAction->SetWindowWidth(21);
		}
		hpAction->SetVKernelSigmaSq(5.0);
		hpAction->SetWindowHeight(31);
		hpAction->SetMode(HighPassFilterAction::StoreRevised);
		changeResAction->Add(hpAction);

		current->Add(changeResAction);

		current = focAction;
		if(calPassband)
		{
			current->Add(new CalibratePassbandAction());
		}
		
		SumThresholdAction *t2 = new SumThresholdAction();
		if(keepTransients)
			t2->SetFrequencyDirectionFlagging(false);
		current->Add(t2);
		
		PlotAction *plotPolarizationStatistics = new PlotAction();
		plotPolarizationStatistics->SetPlotKind(PlotAction::PolarizationStatisticsPlot);
		block.Add(plotPolarizationStatistics);
		
		SetFlaggingAction *setFlagsInAllPolarizations = new SetFlaggingAction();
		setFlagsInAllPolarizations->SetNewFlagging(SetFlaggingAction::PolarisationsEqual);
		
		block.Add(setFlagsInAllPolarizations);
		block.Add(new StatisticalFlagAction());

		bool pedantic = false;
		if(pedantic)
		{
			CombineFlagResults *cfr2 = new CombineFlagResults();
			block.Add(cfr2);
			cfr2->Add(new FrequencySelectionAction());
			if(!keepTransients)
				cfr2->Add(new TimeSelectionAction());
		} else {
			if(!keepTransients)
				block.Add(new TimeSelectionAction());
		}

		BaselineSelectionAction *baselineSelection = new BaselineSelectionAction();
		baselineSelection->SetPreparationStep(true);
		block.Add(baselineSelection);

		if(!clearFlags)
		{
			SetFlaggingAction *orWithOriginals = new SetFlaggingAction();
			orWithOriginals->SetNewFlagging(SetFlaggingAction::OrOriginal);
			block.Add(orWithOriginals);
		}
	}

	void DefaultStrategy::LoadFullStrategy(ActionBlock &destination, enum TelescopeId telescopeId, unsigned flags, double frequency, double timeRes, double frequencyRes)
	{
		ForEachBaselineAction *feBaseBlock = new ForEachBaselineAction();
		destination.Add(feBaseBlock);
		
		LoadStrategy(*feBaseBlock, telescopeId, flags, frequency, timeRes, frequencyRes);

		feBaseBlock->Add(new WriteFlagsAction());

		PlotAction *antennaPlotAction = new PlotAction();
		antennaPlotAction->SetPlotKind(PlotAction::AntennaFlagCountPlot);
		feBaseBlock->Add(antennaPlotAction);

		PlotAction *frequencyPlotAction = new PlotAction();
		frequencyPlotAction->SetPlotKind(PlotAction::FrequencyFlagCountPlot);
		feBaseBlock->Add(frequencyPlotAction);

		BaselineSelectionAction *baselineSelection = new BaselineSelectionAction();
		baselineSelection->SetPreparationStep(false);
		destination.Add(baselineSelection);
	}
	
}
