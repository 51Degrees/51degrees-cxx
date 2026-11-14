/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2026 51 Degrees Mobile Experts Limited, Davidson House,
 * Forbury Square, Reading, Berkshire, United Kingdom RG1 3EU.
 *
 * This Original Work is licensed under the European Union Public Licence
 * (EUPL) v.1.2 and is subject to its terms as set out below.
 *
 * If a copy of the EUPL was not distributed with this file, You can obtain
 * one at https://opensource.org/licenses/EUPL-1.2.
 *
 * The 'Compatible Licences' set out in the Appendix to the EUPL (as may be
 * amended by the European Commission) shall be deemed incompatible for
 * the purposes of the Work and the provisions of the compatibility
 * clause in Article 5 of the EUPL shall not apply.
 *
 * If using the Work as, or as part of, a network application, by
 * including the attribution notice(s) required under Article 5 of the EUPL
 * in the end user terms of the application under an appropriate heading,
 * such notice(s) shall fulfill the requirements of that article.
 * ********************************************************************* */

#ifndef FIFTYONE_DEGREES_PROFILE_META_DATA_BUILDER_IPI_HPP
#define FIFTYONE_DEGREES_PROFILE_META_DATA_BUILDER_IPI_HPP

#include <vector>
#include "../common/ProfileMetaData.hpp"
#include "../common/EntityMetaDataBuilder.hpp"
#include "ipi.h"

namespace FiftyoneDegrees {
	namespace IpIntelligence {
		using FiftyoneDegrees::Common::EntityMetaDataBuilder;
		using FiftyoneDegrees::Common::ProfileMetaData;

		/**
		 * Meta data builder class contains static helper methods used when
		 * building profile meta data instances from an IP Intelligence data set.
		 */
		class ProfileMetaDataBuilderIpi : EntityMetaDataBuilder {
		public:
			/**
			 * Build a new instance of ProfileMetaData from the profile at the
			 * index provided among the data file's profiles. The profile's
			 * offset in the profiles collection is its key, as an IP
			 * Intelligence data file holds no profile ids.
			 * @param dataSet pointer to the data set containing the profile
			 * @param profileIndex index of the profile
			 * @return the new instance, or nullptr when the data set gives no
			 * profile at the index
			 */
			static ProfileMetaData* buildByIndex(
				fiftyoneDegreesDataSetIpi *dataSet,
				uint32_t profileIndex);

			/**
			 * Build a new instance of ProfileMetaData from the profile at the
			 * offset provided, which is its key.
			 * @param dataSet pointer to the data set containing the profile
			 * @param profileOffset offset of the profile in the profiles
			 * collection
			 * @return the new instance, or nullptr when the offset is
			 * UINT32_MAX or the data set gives no profile at it
			 */
			static ProfileMetaData* buildByOffset(
				fiftyoneDegreesDataSetIpi *dataSet,
				uint32_t profileOffset);

			/**
			 * Build a new instance of ProfileMetaData from the underlying
			 * profile provided.
			 * @deprecated Use buildByIndex or buildByOffset, which read the
			 * profile in C. Kept for one release, as a public name.
			 * @param dataSet pointer to the data set containing the profile
			 * @param profile pointer to the underlying profile to create the
			 * meta data from
			 * @param profileOffset offset of the profile in the profiles
			 * collection, which is its key
			 * @return the new instance
			 */
			static ProfileMetaData* build(
				fiftyoneDegreesDataSetIpi *dataSet,
				const fiftyoneDegreesProfile *profile,
				const uint32_t profileOffset);
		};
	}
}

#endif