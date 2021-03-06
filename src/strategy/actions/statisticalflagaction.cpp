#include "../../util/progresslistener.h"

#include "statisticalflagaction.h"

#include "../algorithms/statisticalflagger.h"
#include "../algorithms/siroperator.h"

#include "../control/artifactset.h"

namespace rfiStrategy {

	void StatisticalFlagAction::Perform(ArtifactSet &artifacts, class ProgressListener &)
	{
		TimeFrequencyData &data = artifacts.ContaminatedData();
			
		Mask2DPtr mask = Mask2D::CreateCopy(data.GetSingleMask());
		
		StatisticalFlagger::DilateFlags(mask, _enlargeTimeSize, _enlargeFrequencySize);
		SIROperator::OperateHorizontally(mask, _minimumGoodTimeRatio);
		SIROperator::OperateVertically(mask, _minimumGoodFrequencyRatio);
		data.SetGlobalMask(mask);
	}

} // namespace rfiStrategy
