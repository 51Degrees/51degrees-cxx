"""Temporary. Adds the profile memo to a tree under A4_MEMO so one source can
build both with and without it. Run once on the temporary branch."""
import sys

root = sys.argv[1]


def patch(path, old, new):
    with open(path, 'rb') as f:
        text = f.read().decode('utf-8')
    crlf = '\r\n' in text
    text = text.replace('\r\n', '\n')
    if text.count(old) != 1:
        raise SystemExit('expected one match in %s for %r, found %d' % (path, old[:60], text.count(old)))
    text = text.replace(old, new)
    if crlf:
        text = text.replace('\n', '\r\n')
    with open(path, 'wb') as f:
        f.write(text.encode('utf-8'))


ipih = root + '/src/ipi/ipi.h'
ipic = root + '/src/ipi/ipi.c'

patch(ipih, """							result has not been populated */
} fiftyoneDegreesResultIpi;""", """							result has not been populated */
#ifdef A4_MEMO
	fiftyoneDegreesCollectionItem memoItem;
#endif
} fiftyoneDegreesResultIpi;""")

patch(ipic, """static void resultsIpiRelease(ResultsIpi* results) {
""", """#ifdef A4_MEMO
static void resultsIpiMemoRelease(ResultsIpi* results) {
	for (uint32_t i = 0; i < results->capacity; i++) {
		Item* const item = &results->items[i].memoItem;
		if (item->data.ptr != NULL) {
			COLLECTION_RELEASE(item->collection, item);
			DataReset(&item->data);
			item->handle = NULL;
			item->collection = NULL;
		}
	}
}
#endif

static void resultsIpiRelease(ResultsIpi* results) {
""")

patch(ipic, """		// Reset the property and values list ready for first use sized for
		// a single value to be returned.
""", """#ifdef A4_MEMO
		for (uint32_t i = 0; i < results->capacity; i++) {
			DataReset(&results->items[i].memoItem.data);
			results->items[i].memoItem.handle = NULL;
			results->items[i].memoItem.collection = NULL;
		}
#endif
		// Reset the property and values list ready for first use sized for
		// a single value to be returned.
""")

patch(ipic, """void fiftyoneDegreesResultsIpiFree(fiftyoneDegreesResultsIpi* results) {
	resultsIpiRelease(results);
""", """void fiftyoneDegreesResultsIpiFree(fiftyoneDegreesResultsIpi* results) {
#ifdef A4_MEMO
	resultsIpiMemoRelease(results);
#endif
	resultsIpiRelease(results);
""")

patch(ipic, """	// Reset the results data before iterating the evidence.
	results->count = 0;
	resultsIpiResetHasValuesCache(results);

	addResultsFromIpAddressNoChecks(
""", """	// Reset the results data before iterating the evidence.
#ifdef A4_MEMO
	resultsIpiMemoRelease(results);
#endif
	results->count = 0;
	resultsIpiResetHasValuesCache(results);

	addResultsFromIpAddressNoChecks(
""")

patch(ipic, """			if (!result->graphResult.isGroupOffset) {
				const uint32_t profileOffsetValue = getProfileOffset(
					dataSet->profileOffsets,
					result->graphResult.offset,
					exception);

				if (EXCEPTION_OKAY) {
""", """			if (!result->graphResult.isGroupOffset) {
#ifdef A4_MEMO
				Profile* profile = (Profile*)result->memoItem.data.ptr;
				if (profile == NULL) {
					const uint32_t memoOffset = getProfileOffset(
						dataSet->profileOffsets,
						result->graphResult.offset,
						exception);
					if (EXCEPTION_OKAY && memoOffset != NULL_PROFILE_OFFSET) {
						const CollectionKey memoKey = {
							memoOffset,
							CollectionKeyType_Profile,
						};
						profile = (Profile*)dataSet->profiles->get(
							dataSet->profiles,
							&memoKey,
							&result->memoItem,
							exception);
					}
				}
				if (profile != NULL && EXCEPTION_OKAY) {
					count += addValuesFromProfile(
						(DataSetIpi*)dataSet,
						results,
						profile,
						property,
						FULL_RAW_WEIGHTING,
						exception);
				}
				return count;
#endif
				const uint32_t profileOffsetValue = getProfileOffset(
					dataSet->profileOffsets,
					result->graphResult.offset,
					exception);

				if (EXCEPTION_OKAY) {
""")
print('patched')
