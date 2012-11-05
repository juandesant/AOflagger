/** @file aoflagger.h @brief Main AOFlagger include file.
 * @author André Offringa offringa@gmail.com
 */

#ifndef AOFLAGGER_INTERFACE_H
#define AOFLAGGER_INTERFACE_H

#include <cstring>
#include <string>

/** @brief Contains all the public types used by the AOFlagger.
 * 
 * See the @ref AOFlagger class description for details.
 * @author André Offringa offringa@gmail.com
 */
namespace aoflagger {

	/** @brief Strategy identifier for the supported telescopes.
	 * 
	 * If you have an optimized strategy for an unlisted telescope, please
	 * send me an e-mail or give me a call!
	 * @sa AOFlagger::MakeStrategy().
	 */
	enum TelescopeId {
		/** @brief Most generic strategy. */
		GENERIC_TELESCOPE,
		/** @brief Default strategy for the LOFAR (Low-Frequency Array in Europe). */
		LOFAR_TELESCOPE,
		/** @brief Default strategy for the MWA (Murchison Widefield Array in Western Australia). */
		MWA_TELESCOPE,
		/** @brief Default strategy for the WSRT (Westerbork Synthesis Radio Telescope in the Netherlands). */
		WSRT_TELESCOPE
	};

	/** @brief Lists the flags that can be used to alter a default strategy.
	 * 
	 * These flags can have different effects
	 * on strategies for different telescopes. Some might only
	 * have effect for specific telescopes, i.e., specific values
	 * of @ref TelescopeId. Flags can be combined with the arithmetic
	 * 'OR' ('|') operator.
	 * @sa AOFlagger::MakeStrategy()
	 */
	class StrategyFlags
	{
		public:
			/** @brief No flags: use the default strategy for the telescope. */
			static const unsigned NONE;
			
			/** @brief Optimize for telescope's lower frequencies. */
			static const unsigned LOW_FREQUENCY;
			
			/** @brief Optimize for telescope's higher frequencies. */
			static const unsigned HIGH_FREQUENCY;
			
			/** @brief Make strategy insensitive for transient effect.
			 * 
			 * This would make the strategy insensitive to RFI that is broadband but
			 * rapidly changes over time. This could be useful when searching for
			 * transients. Note that many celestial transients, such as most pulsars,
			 * are not strong enough to be noticable on high resolution, and the
			 * loss in RFI sensitivity is quite severe, so it is advisable to play around
			 * with strategies when optimizing for transient detections. */
			static const unsigned TRANSIENTS;
			
			/** @brief Increase robustness by decreasing convergence speed.
			 *
			 * This flag trades flagging speed for better convergence properties, which
			 * might be useful when having a large dynamic range in RFI or the default
			 * strategy is not working well. Opposite of @ref FAST.
			 */
			static const unsigned ROBUST;
			
			/** @brief Optimize for speed at cost of accuracy and robustness. */
			static const unsigned FAST;
			
			/** @brief Optimize for strong off-axis source in the observation.
			 * 
			 * Strong off-axis sources can create strong rapid fringes, which
			 * (depending on observation resolution) might trigger the flagger
			 * falsely. This is especially the case for widefield telescopes
			 * where each station has a large collecting area, e.g.
			 * the LOFAR LBA stations. */
			static const unsigned OFF_AXIS_SOURCES;
			
			/** @brief Make the strategy less sensitive to RFI than the default
			 * telescope settings.
			 * 
			 * This creates also more false positives, but might be useful if the
			 * default setting seems to leave too much RFI in. Opposite of
			 * @ref UNSENSITIVE.
			 */
			static const unsigned UNSENSITIVE;
			
			/** @brief Make the strategy more sensitive to RFI than the default
			 * telescope settings. 
			 * 
			 * Can be used if the flagger seems to destroy too much data. In this
			 * case, you might consider using @ref ROBUST instead.
			 */
			static const unsigned SENSITIVE;
			
			/** @brief Will keep the background images in memory so they can be displayed
			 * in the GUI. */
			static const unsigned GUI_FRIENDLY;
			
			/** @brief Erase any flags that are already set.
			 * 
			 * If this flag is not specified, the flags that have already been set will
			 * be combined with the flags found by the flagger.
			 */
			static const unsigned CLEAR_FLAGS;
			
		private:
			StrategyFlags();
	};

	/** @brief A set of time-frequency 'images' which together contain data for one
	 * correlated baseline or dish.
	 * 
	 * The class either holds 1, 2, 4 or 8 images. These images have time on the
	 * x-axis (most rapidly changing index) and frequency on the y-axis. The
	 * cells specify flux levels, which do not need to have been calibrated.
	 * 
	 * If the set contains only one image, it specifies amplitudes of a single
	 * polarization. If it contains two images, it specifies the real and imaginary
	 * parts of a single polarization. With four images, it contains the real
	 * and imaginary values of two polarizations (ordered real pol A, imag pol A,
	 * real pol B, imag pol B). With eight images, it contains complex values for
	 * four correlated polarizations (ordered real pol A, imag pol A, real pol B,
	 * ... etc).
	 * 
	 * @note When accesses the image data, note that there might be more items on one row
	 * than the width of the image. The rows are padded to align them e.g. for
	 * SSE instructions. Use @ref HorizontalStride() to get the actual number of
	 * floats per row. 
	 */
	class ImageSet
	{
		public:
			friend class AOFlagger;
			
			/** @brief Copy the image set. Only references to images are copied. */
			ImageSet(const ImageSet& sourceImageSet);
			
			/** @brief Destruct image set. Destroys its images if no longer referenced. */
			~ImageSet();
			
			/** @brief Assign to this image set. Only references to images are copied. */
			ImageSet &operator=(const ImageSet& sourceImageSet);
			
			/** @brief Get access to the data buffer of an image.
			 * @param imageIndex Index of image. See class description for ordering.
			 * \note Rows are padded, see @ref HorizontalStride().
			 */
			float *ImageBuffer(size_t imageIndex);
			
			/** @brief Get constant access to the data buffer of an image.
			 * @param imageIndex Index of image. See class description for ordering.
			 */
			const float *ImageBuffer(size_t imageIndex) const;
			
			/** @brief Get width (number of time steps) of images. */
			size_t Width() const;
			
			/** @brief Get height (number of frequency channels) of images. */
			size_t Height() const;
			
			/** @brief Get number of images, see class description for details. */
			size_t ImageCount() const;
			
			/** @brief Get total number of floats in one row.
			 * 
			 * Row might have been padded to allow for
			 * SSE instructions and other optimizations. Therefore, one should
			 * add the horizontal stride to a data pointer to get the float in the next
			 * row (channel).
			 * 
			 * Example:
			 * @code{.cpp}
			 *(ImageSet::ImageBuffer(imageIndex) + x + y * ImageSet::HorizontalStride()) 
			 * @endcode
			 * will return the value at position x,y.
			 */
			size_t HorizontalStride() const;
			
		private:
			ImageSet(size_t width, size_t height, size_t count);
			
			ImageSet(size_t width, size_t height, size_t count, float initialValue);
			
			static void assertValidCount(size_t count);
			
			class ImageSetData *_data;
	};

	/** @brief Holds a flagging strategy.
	 * 
	 * Telescope-specific flagging strategies can be created with
	 * @ref AOFlagger::MakeStrategy(), or
	 * can be loaded from disc with @ref AOFlagger::LoadStrategy(). Strategies
	 * can not be changed with this interface. A user can create stored strategies
	 * with the @c rfigui tool that is part of the aoflagger package.
	 */
	class Strategy
	{
		public:
			friend class AOFlagger;
			
			/** @brief Create a copy of a strategy. */
			Strategy(const Strategy& sourceStrategy);
			
			/** @brief Destruct strategy. */
			~Strategy();
			
			/** @brief Assign to strategy. */
			Strategy &operator=(const Strategy& sourceStrategy);
			
		private:
			Strategy(enum TelescopeId telescopeId, unsigned strategyFlags, double frequency=0.0, double timeRes=0.0, double frequencyRes=0.0);
			
			Strategy(const std::string& filename);
			
			class StrategyData *_data;
	};

	/** @brief A two-dimensional flag mask.
	 * 
	 * The flag mask specifies which values in an @ref ImageSet are flagged.
	 * A value @c true means a value is flagged, i.e., contains RFI and should
	 * not be used in further data processing (calibration, imaging, etc.).
	 * A flag denotes that all the value at that time-frequency position should
	 * be ignored for all polarizations. This makes sense, because if one
	 * polarization has seen RFI, all polarizations are probably affected. Also,
	 * solving for Stokes matrices during calibration might not work well when
	 * the polarizations are not flagged equally.
	 * 
	 * If polarization-specific flags are needed, one could run the flagger on
	 * each polarization individually. However, note that some algorithms, like
	 * the morphological scale-invariant rank operator (SIR operator), work best
	 * when seeing the flags from all polarizations.
	 * 
	 * @note When accesses the flag data, note that there might be more items on one row
	 * than the width of the mask. The rows are padded to align them e.g. for
	 * SSE instructions. Use @ref HorizontalStride() to get the actual number of
	 * bools per row. 
	 */
	class FlagMask
	{
		public:
			friend class AOFlagger;
			
			/** @brief Copy a flag mask. Only copies a reference, not the data. */
			FlagMask(const FlagMask& sourceMask);
			
			/** @brief Destroy a flag mask. Destroys mask data if no longer references. */
			~FlagMask();
			
			/** @brief Get the width of the mask. */
			size_t Width() const;
			
			/** @brief Get the height of the mask. */
			size_t Height() const;
			
			/** @brief Get total number of bools in one row.
			 * 
			 * Row might have been padded to allow for
			 * SSE instructions and other optimizations. Therefore, one should
			 * add the horizontal stride to a data pointer to get the flags in
			 * the next row (channel).
			 * 
			 * Example:
			 * @code{.cpp}
			 *(FlagMask::Buffer() + x + y * Buffer::HorizontalStride()) 
			 * @endcode
			 * will return the flag value at position x,y.
			 */
			size_t HorizontalStride() const;
			
			/** @brief Get access to the data buffer. */
			bool *Buffer();
			
			/** @brief Get constant access to the data buffer. */
			const bool *Buffer() const;
			
		private:
			FlagMask();
			
			class FlagMaskData *_data;
	};

	/** @brief Statistics that can be collected online and saved to a measurement set.
	 * 
	 * It is useful to collect some statistics during flagging, because all data goes through
	 * memory at highest resolution. This class contains the collected statistics and
	 * some meta data required for collecting. It can be created with
	 * @ref AOFlagger::MakeQualityStatistics(). Statistics can be added to it with
	 * @ref AOFlagger::CollectStatistics(), and saved to disk with
	 * @ref AOFlagger::WriteStatistics().
	 * 
	 * This class does not allow viewing or modifying statistics, it only contains the most
	 * basic form to collect statistics during flagging and writing them in the (well-defined)
	 * quality statistic tables format. These statistics can be viewed interactively with
	 * the @c aoqplot tool.
	 * 
	 * Collecting statistics is not as expensive as flagging, but still takes some time, so it
	 * is recommended to use multiple threads for collecting as well. This class is however not
	 * thread save, but it is okay to use different QualityStatistics objects from different
	 * thread contexts. During finalization, the different objects can be combined with the
	 * @ref operator+=() method, and then in full written to the measurement set.
	 */
	class QualityStatistics
	{
		public:
			friend class AOFlagger;
			
			/** @brief Copy the object. This is fast; only references are copied. */
			QualityStatistics(const QualityStatistics &sourceQS);
			
			/** @brief Destruct the object. Data is destroyed if no more references exist. */
			~QualityStatistics();
			
			/** @brief Assign to this object. This is fast; only references are copied. */
			QualityStatistics &operator=(const QualityStatistics &sourceQS);
			
			/** @brief Combine the statistics from the given object with the statistics in this object.
			 *
			 * This is a relative expensive operation, so should only be used scarsely. It can be used
			 * to combine the results of different threads, as explained in the class description.
			 * 
			 * It is okay to combine quality statistics with different meta data (scan time count, channel
			 * count, etc.). When using this object again during collecting (see @ref CollectStatistics()),
			 * after combining it with another object, it will still use the meta data it was initialized with.
			 */
			QualityStatistics &operator+=(const QualityStatistics &rhs);
			
		private:
			QualityStatistics(const double *scanTimes, size_t nScans, const double *channelFrequencies, size_t nChannels, size_t nPolarizations);
			
			class QualityStatisticsData *_data;
	};
	
	/** @brief Main class for access to the flagger functionality.
	 * 
	 * Software using the flagger should first create an instance of the @ref AOFlagger
	 * class, which initializes the flagger, allows making data buffers that are suitable
	 * to pass to the flagger and allows to specify the strategy.
	 * 
	 * To flag a data set:
	 * - Create an AOFlagger
	 * - Specify a strategy with MakeStrategy() or LoadStrategy()
	 * - Make a data buffer with MakeImageSet()
	 * - Fill the images with your data
	 * - Call Run() with the created Strategy and ImageSet
	 * - Process the data that was returned in the FlagMask.
	 * 
	 * To flag multiple baselines, the Strategy can be stored and the same buffer can be used.
	 * The Run() function is thread-safe, as long as different ImageSet instances are specified.
	 * It is okay to call Run() from different threads with the same Strategy, and it is
	 * recommended to do so for multi threaded implementations.
	 * 
	 * It is okay to create multiple AOFlagger instances, but not recommended.
	 */
	class AOFlagger
	{
		public:
			/** @brief Create and initialize the flagger main class. */
			AOFlagger();
			
			/** @brief Destructor. */
			~AOFlagger();
			
			/** @brief Create a new uninitialized @ref ImageSet with specified specs.
			 * 
			 * The float values will not be initialized.
			 * @param width Number of time steps in images
			 * @param height Number of frequency channels in images
			 * @param count Number of images in set (see class description
			 * of @ref ImageSet for image order).
			 * @return A new ImageSet.
			 */
			ImageSet MakeImageSet(size_t width, size_t height, size_t count);
			
			/** @brief Create a new initialized @ref ImageSet with specified specs.
			 * @param width Number of time steps in images
			 * @param height Number of frequency channels in images
			 * @param count Number of images in set (see class description
			 * of @ref ImageSet for image order).
			 * @param initialValue Initialize all pixels with this value.
			 * @return A new ImageSet.
			 */
			ImageSet MakeImageSet(size_t width, size_t height, size_t count, float initialValue);
			
			/** @brief Initialize a strategy for a specific telescope.
			 * 
			 * All parameters are hints to optimize the strategy, but need not actual alter the
			 * strategy (or even have correct effects), but the returned strategy should for most
			 * common cases be fine. Some properties conflict, e.g., specifying @ref StrategyFlags::LOW_FREQUENCY
			 * as flag and giving a high @a frequency value. In these cases it is not defined which
			 * parameter takes precedence, thus it should obviously be avoided.
			 * 
			 * If frequency, time resolution or frequency resolution or not known, they can be left at
			 * their default values. Currently, they have no effect, but might have effect in later
			 * versions. Therefore, if they are known, it is recommended to specify them. They could
			 * even identify problematic cases and report this.
			 * 
			 * @param telescopeId Identifies the telescope to optimize the strategy for.
			 * @param strategyFlags Flags to optimize the strategy further.
			 * @param frequency The observation frequency in Hz, or zero if unknown.
			 * @param timeRes The time resolution (distance between to consecutive time steps) in s, or zero if unknown.
			 * @param frequencyRes The frequency resolution (distance between to channels) in Hz, or zero if unknown.
			 */
			Strategy MakeStrategy(enum TelescopeId telescopeId=GENERIC_TELESCOPE, unsigned strategyFlags=StrategyFlags::NONE, double frequency=0.0, double timeRes=0.0, double frequencyRes=0.0);
			
			/** @brief Load a strategy from disk.
			 * 
			 * The best way to create strategies is to use the @c rfigui tool. In case you have optimized
			 * strategies for an unlisted telescope or for new parameters, please let me know so that I
			 * can further optimize the flagger.
			 * @param filename Full pathname to .rfis strategy file.
			 * @return The new @ref Strategy.
			 */
			Strategy LoadStrategy(const std::string& filename);
			
			/** @brief Run the flagging strategy on the given data.
			 * 
			 * Ok to call from multiple threads as long as it is called
			 * with a different @a input parameter.
			 * @param strategy The flagging strategy that will be used.
			 * @param input The data to run the flagger on.
			 * @return The flags identifying bad (RFI contaminated) data.
			 */
			FlagMask Run(Strategy& strategy, ImageSet& input);
			
			/** @brief Create a new object for collecting statistics.
			 * 
			 * See the QualityStatistics class description for info on multithreading and/or combining statistics
			 * with different meta data. The meta data that is passed to this method will be used for all
			 * calls to CollectStatistics() if this class is specified.
			 */
			QualityStatistics MakeQualityStatistics(const double *scanTimes, size_t nScans, const double *channelFrequencies, size_t nChannels, size_t nPolarizations);
			
			/** @todo implement */
			void CollectStatistics(QualityStatistics& destination, const ImageSet& imageSet, const FlagMask& rfiFlags, const FlagMask& correlatorFlags, size_t antenna1, size_t antenna2);
			
			/** @todo implement */
			void WriteStatistics(const QualityStatistics& statistics, const std::string& measurementSetPath);
			
		private:
			/** @brief It is not allowed to copy this class
			 */
			AOFlagger(const AOFlagger &source) { }
			
			/** @brief It is not allowed to assign to this class
			 */
			void operator=(const AOFlagger &source) { }
	};

}

#endif