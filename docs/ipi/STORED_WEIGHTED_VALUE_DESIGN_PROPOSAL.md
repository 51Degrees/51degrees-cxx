# Proposal: Support Weighted Values in the IPI Dataset (C Reader Side)

## Overview

This document describes the implementation plan for reading **weighted values** in the IPI dataset on the C/C++ reader side. The approach follows the Pearl export-side design documented in `Pearl/WEIGHTED_VALUE_EXPORT_DESIGN.md`.

The weight is stored in the **Value record** itself — the existing `urlOffset` field is repurposed as a dual-purpose `urlOffsetOrWeight` field. When the upper two bytes match the mask pattern `0xFF00`, the lower two bytes encode the weight as a `uint16_t` (0–65535, representing 0.0–1.0). When the mask is not present, the field is a URL offset or the legacy `-1` sentinel (existing behavior). Weighted properties use the existing **list property** mechanism (`isList = true`), with each (value, weight) pair being a separate Value record.

This is a much simpler design than the previous proposal (which used a compound blob in the strings collection with a collection decorator). No decorator, no compound stored values, no sub-references within the strings collection.

## Background

### Property Type System

Properties in the IPI dataset have two type descriptors:

- **`valueType`** (in `property_t`): The conceptual type of the property value as returned by the API. Inherited from Device Detection.
- **`storedValueType`** (in `property_type_record_t`): The physical format of the value in the data file. Stored in the separate `propertyTypes` collection, which extends `property_t` for IPI-specific needs.

### Weight in the Value Record

The weight lives in the **Value record**, not in the strings collection:

- The `urlOffset` field in the C `Value` struct becomes `urlOffsetOrWeight` (signed `int32_t`)
- The field uses a **masked encoding** to distinguish weights from URL offsets:
  - If the upper 2 bytes equal `0xFF00` (i.e., `(value & 0xFFFF0000) == 0xFF000000`) → the lower 2 bytes are a `uint16_t` weight (0–65535, representing 0.0–1.0)
  - Otherwise → it is a URL offset (existing behavior), including the legacy `-1` sentinel for "no URL"
- **Why not simple negative numbers?** The value `-1` (`0xFFFFFFFF`) is already used as a sentinel meaning "no URL" in both device-detection-cxx and ip-intelligence-cxx (see `ValueMetaDataBuilderHash.cpp:51`, `ValueMetaDataBuilderIpi.cpp:116`). A simple "negative = weight" scheme would collide with this existing convention.
- **Why `0xFF00` mask?** The pattern `0xFF00` in the upper 2 bytes (`0xFF00xxxx`) creates a distinct range that does not collide with `-1` (`0xFFFFFFFF`, which has `0xFFFF` in the upper bytes) nor with any valid non-negative URL offset. The `isMasked()` check reliably distinguishes all three cases.
- **Working range:** Masked values span from `0xFF000000` (int32: -16777216, weight=0.0) to `0xFF00FFFF` (int32: -16711681, weight=1.0)
- Weighted properties have `isList = true` — multiple Value records per profile per property
- Each unique (value, weight) combination is a separate Value record in the Values collection
- The string/int/float data in the strings collection remains **standard** — no compound blobs

### Mask Encoding/Decoding Reference

```c
// Pack: encode a uint16_t weight into a masked int32_t
int32_t packWeight(uint16_t weight) {
    return (int32_t)((0xFF00 << 16) | weight);
}

// Unpack: extract uint16_t weight from a masked int32_t
uint16_t unpackWeight(int32_t value) {
    return (uint16_t)(value & 0xFFFF);
}

// Check: does this int32_t carry a masked weight?
bool isMasked(int32_t value) {
    return ((value & 0xFFFF0000) == 0xFF000000);
}

// Float conversion: weight float [0.0, 1.0] → uint16_t [0, 65535]
uint16_t floatToUshort(float w) { return (uint16_t)roundf(w * UINT16_MAX); }

// Reverse: uint16_t [0, 65535] → double [0.0, 1.0]
double ushortToDouble(uint16_t w) { return (double)w / UINT16_MAX; }
```

**Examples:**
| Float weight | uint16_t | Packed int32_t (hex) | Packed int32_t (decimal) |
|---|---|---|---|
| 0.00 | 0 | `0xFF000000` | -16777216 |
| 0.15 | 9830 | `0xFF002666` | -16767386 |
| 1.00 | 65535 | `0xFF00FFFF` | -16711681 |

**Collision check:**
| Value | Upper bytes | `isMasked()`? |
|---|---|---|
| Encoded weight (e.g. 0.15) | `0xFF00` | **true** |
| `-1` (no URL sentinel) | `0xFFFF` | **false** |
| `27` (valid URL offset) | `0x0000` | **false** |

### How It Works with List Properties

List properties are an existing mechanism. When `isList = true`, a profile can have **multiple values** for that property. The profile stores value **indices** (not inline data), and the existing iteration infrastructure (`ProfileIterateValuesForProperty`) already handles multiple values per property.

For a weighted property like "Country" on an IP profile:

```
Values collection:
  Value[100]: { propertyIndex=5, nameOffset→"US",  urlOffsetOrWeight=0xFF00A666 (-16741786) }  // weight≈0.65
  Value[101]: { propertyIndex=5, nameOffset→"GB",  urlOffsetOrWeight=0xFF005999 (-16761959) }  // weight≈0.35
  Value[102]: { propertyIndex=5, nameOffset→"DE",  urlOffsetOrWeight=0xFF0001D1 (-16776751) }  // weight≈0.007

Profile 42:
  valueIndices = [..., 100, 101, 102, ...]
```

Each weighted string is a separate Value record. The profile references all of them. The existing `addValueWithPercentage` callback in `ipi.c` already iterates over all values for a property — it just needs to extract the weight from the Value record instead of (or in addition to) using the profile-level weight.

### Value Data Sharing

Multiple Value records can share the same `nameOffset` pointing to a single interned string in the strings collection. "US" is stored once; Value records that differ only in weight all point to the same `nameOffset`. The strings collection does **not** grow with weight permutations — only the Values collection does.

Value records themselves are also shared across profiles. If two profiles both have "US" with weight 3000, they reference the same Value record by index.

## Binary Format Change

### Current Value Binary Format

```
[int16  propertyIndex]
[int32  nameOffset]          → offset into strings collection
[int32  descriptionOffset]   → offset into strings collection
[int32  urlOffset]           → offset into strings collection (currently treated as unsigned)
```

### New Value Binary Format

```
[int16  propertyIndex]
[int32  nameOffset]          → offset into strings collection (unchanged)
[int32  descriptionOffset]   → offset into strings collection (unchanged)
[int32  urlOffsetOrWeight]   → URL offset (or -1 for none), OR masked weight if upper 2 bytes == 0xFF00
```

The last field becomes explicitly **signed** `int32_t`. Both are 4 bytes — binary compatible. When Pearl writes a weighted value, it packs the weight using the `0xFF00` mask: `(int32_t)((0xFF00 << 16) | uint16_weight)`. The C reader detects the mask and extracts the lower 2 bytes.

## Architecture Decisions

### Decision 1: Weight in the Value Record, Not Strings Collection

The weight lives in the Value's `urlOffsetOrWeight` field. The strings collection stores only the value data (string, int, float, etc.) — no compound blobs, no sub-references.

**Rationale:** Least risk, minimal structural change. Weight can be read from the Value record without consulting the property type. Works for any value type (string, int, double, etc.), not just strings.

**Trade-offs:**
- Cannot add a URL to a weighted value (acceptable)
- More Value records: each weighted property may have up to 65536 weight levels per distinct value. But Value records are small (14 bytes) and shared across profiles.

### Decision 2: Leverage Existing List Property Mechanism

Weighted values use `isList = true`. Multiple Value records for the same property in a profile are already handled by `ProfileIterateValuesForProperty`. The `addValueWithPercentage` callback receives each Value separately — no special iteration logic needed.

**Rationale:** Reuses proven infrastructure. The profile reader already groups values by property and iterates them. No new iteration patterns needed.

### Decision 3: Rename Profile-Specific Types to Generic Names and Lift to common-cxx

The existing `ProfilePercentage` and `IpiList` types are generic containers for weighted collection items and should be named `WeightedItem` / `WeightedItemList`. They belong in **common-cxx** since they are general-purpose data structures.

### Decision 4: Weighted Values Supported for Any Type

The `urlOffsetOrWeight` approach works for any value type — `WeightedString`, `WeightedInt`, `WeightedDouble`, etc. The `storedValueType` on the Property indicates whether values carry weights. The underlying stored data in the strings collection is the same type as the non-weighted variant (e.g., a `WeightedString` stores plain strings, a `WeightedInt` stores plain ints). The weight is orthogonal to the value data.

## Type Renames

| Current Name | New Name | New Location | Scope |
|---|---|---|---|
| `fiftyoneDegreesProfilePercentage` | `fiftyoneDegreesWeightedItem` | **common-cxx** (`weightedItem.h`) | Public API |
| `fiftyone_degrees_profile_percentage_t` | `fiftyone_degrees_weighted_item_t` | common-cxx | Struct tag |
| `fiftyoneDegreesIpiList` | `fiftyoneDegreesWeightedItemList` | **common-cxx** (`weightedItem.h`) | Public API |
| `fiftyone_degrees_ipi_list_t` | `fiftyone_degrees_weighted_item_list_t` | common-cxx | Struct tag |
| `addValueWithPercentage` | `addWeightedValue` | ip-intelligence-cxx (stays in ipi.c) | Static in ipi.c |
| `stateWithPercentage` | `stateWithWeighting` | ip-intelligence-cxx (stays in ipi.c) | Static in ipi.c |
| `profilePercentageItem` (local vars) | `weightedItem` | ip-intelligence-cxx | Local variables |
| `ProfilePercentage` (MAP_TYPE) | `WeightedItem` | ip-intelligence-cxx (fiftyone.h) | MAP_TYPE macro |
| `ResultsIpiGetValues` return type | `const fiftyoneDegreesWeightedItem*` | ip-intelligence-cxx | Public API |

**`rawWeighting`** is kept as-is.

Updated struct definition (now in common-cxx):

```c
/**
 * A weighted collection item. Represents a value retrieved from the
 * dataset together with a weight indicating the proportion or
 * confidence of this value (out of 65535). Used for profile-level
 * weighting (from profile groups) where the weight indicates the
 * proportion of the matched IP range attributable to this profile.
 */
typedef struct fiftyone_degrees_weighted_item_t {
    fiftyoneDegreesCollectionItem item;
    uint16_t rawWeighting;
} fiftyoneDegreesWeightedItem;

/**
 * A resizable list of weighted items.
 */
typedef struct fiftyone_degrees_weighted_item_list_t {
    fiftyoneDegreesWeightedItem *items;  /**< Array of weighted items */
    uint32_t count;                       /**< Number of items in use */
    uint32_t capacity;                    /**< Allocated capacity */
} fiftyoneDegreesWeightedItemList;
```

**Files affected by rename:**
- **common-cxx** (new home):
  - `weightedItem.h` (new) — type definitions for `WeightedItem` and `WeightedItemList`
  - `weightedItem.c` (new) — list management functions (init, release, extend, add)
- **ip-intelligence-cxx** (consumer, update references):
  - `ipi.h` — remove old type definitions, include new common-cxx header, update `FIFTYONE_DEGREES_RESULTS_IPI_MEMBERS` macro
  - `ipi.c` — update all internal usage (~20 references), rename static functions
  - `ResultsIpi.cpp` — update C++ layer (~10 references)
  - `ipi_weighted_results.c` — update weighted value construction (~3 references)
  - `fiftyone.h` — update `MAP_TYPE` macro
  - `ResultsIpi.i` — SWIG interface (if it references the C type directly)

## Implementation Plan

### Step 1: Rename `urlOffset` to `urlOffsetOrWeight` in Value Struct (common-cxx)

**`value.h`** — Change the Value struct:

```c
#pragma pack(push, 2)
typedef struct fiftyoneDegrees_value_t {
    const int16_t propertyIndex;       /**< Index of the property the value relates to */
    const int32_t nameOffset;          /**< The offset in the strings structure to the
                                           value name */
    const int32_t descriptionOffset;   /**< The offset in the strings structure to the
                                           value description */
    const int32_t urlOffsetOrWeight;   /**< The offset in the strings structure to the
                                           value URL, or a masked weight if upper 2
                                           bytes == 0xFF00. See ValueIsWeighted(). */
} fiftyoneDegreesValue;
#pragma pack(pop)
```

**`value.h` / `value.c`** — Update `ValueGetUrl` to check for mask:

```c
// Internal helper: check if the value carries a masked weight
#define VALUE_IS_MASKED(v) \
    (((v)->urlOffsetOrWeight & 0xFFFF0000) == (int32_t)0xFF000000)

EXTERNAL const fiftyoneDegreesString* fiftyoneDegreesValueGetUrl(
    const fiftyoneDegreesCollection *strings,
    const fiftyoneDegreesValue *value,
    fiftyoneDegreesCollectionItem *item,
    fiftyoneDegreesException *exception) {
    // Weighted values (masked) have no URL
    if (VALUE_IS_MASKED(value)) {
        return NULL;
    }
    // Legacy -1 sentinel also means no URL
    if (value->urlOffsetOrWeight == -1) {
        return NULL;
    }
    return fiftyoneDegreesStringGet(
        strings,
        value->urlOffsetOrWeight,
        item,
        exception);
}
```

**Add weight accessor:**

```c
/**
 * Gets the weight from a Value record as a uint16_t (0–65535),
 * or 0 if the value is not weighted.
 * The weight is stored in the lower 2 bytes of urlOffsetOrWeight
 * when the upper 2 bytes match the 0xFF00 mask.
 * To convert to a proportion: (double)weight / UINT16_MAX
 * @param value the value to get the weight from
 * @return the weight (0–65535 range), or 0 if not weighted
 */
EXTERNAL uint16_t fiftyoneDegreesValueGetWeight(
    const fiftyoneDegreesValue *value);

/**
 * Returns true if the value carries a masked weight.
 * A value is weighted when (urlOffsetOrWeight & 0xFFFF0000) == 0xFF000000.
 * This is distinct from -1 (0xFFFFFFFF, "no URL" sentinel) and from
 * valid non-negative URL offsets.
 */
EXTERNAL bool fiftyoneDegreesValueIsWeighted(
    const fiftyoneDegreesValue *value);
```

Implementation:

```c
uint16_t fiftyoneDegreesValueGetWeight(
    const fiftyoneDegreesValue *value) {
    if (VALUE_IS_MASKED(value)) {
        return (uint16_t)(value->urlOffsetOrWeight & 0xFFFF);
    }
    return 0;
}

bool fiftyoneDegreesValueIsWeighted(
    const fiftyoneDegreesValue *value) {
    return VALUE_IS_MASKED(value);
}
```

**Reading the weight — step by step:**

1. Read `urlOffsetOrWeight` as `int32_t` from the Value record
2. Check if masked: `(urlOffsetOrWeight & 0xFFFF0000) == 0xFF000000`
   - If `false` → not a weighted value (it's a URL offset or `-1` sentinel)
   - If `true` → proceed to step 3
3. Extract the `uint16_t` weight: `(uint16_t)(urlOffsetOrWeight & 0xFFFF)`
4. Convert to proportion if needed: `(double)weight / UINT16_MAX`

**Precision:** The uint16_t encoding gives 65536 discrete levels with a maximum error of ~1.5×10⁻⁵ (approx 0.0015%), which is more than sufficient for weight/probability data.

### Step 2: Lift WeightedItem / WeightedItemList Types to common-cxx

Move the `WeightedItem` and `WeightedItemList` type definitions and their list management functions (`init`, `release`, `extend`, `add`) from `ipi.h`/`ipi.c` to common-cxx. These are generic data structures with no IPI-specific logic.

**New `weightedItem.h` in common-cxx:**

```c
#include "collection.h"

typedef struct fiftyone_degrees_weighted_item_t {
    fiftyoneDegreesCollectionItem item;
    uint16_t rawWeighting;
} fiftyoneDegreesWeightedItem;

typedef struct fiftyone_degrees_weighted_item_list_t {
    fiftyoneDegreesWeightedItem *items;
    uint32_t count;
    uint32_t capacity;
} fiftyoneDegreesWeightedItemList;

EXTERNAL void fiftyoneDegreesWeightedItemListInit(
    fiftyoneDegreesWeightedItemList *list,
    uint32_t initialCapacity);

EXTERNAL void fiftyoneDegreesWeightedItemListRelease(
    fiftyoneDegreesWeightedItemList *list);

EXTERNAL fiftyoneDegreesStatusCode fiftyoneDegreesWeightedItemListExtend(
    fiftyoneDegreesWeightedItemList *list);

EXTERNAL fiftyoneDegreesWeightedItem* fiftyoneDegreesWeightedItemListAdd(
    fiftyoneDegreesWeightedItemList *list,
    fiftyoneDegreesException *exception);
```

### Step 3: Update ip-intelligence-cxx to Use common-cxx Types

**`ipi.h`:** Remove old `ProfilePercentage` and `IpiList` definitions. Include the new common-cxx header. Update `FIFTYONE_DEGREES_RESULTS_IPI_MEMBERS` macro and `ResultsIpiGetValues` to use `WeightedItem` / `WeightedItemList`.

**`ipi.c`:** Rename all internal references (~20 occurrences). Rename static functions (`addValueWithPercentage` → `addWeightedValue`, `stateWithPercentage` → `stateWithWeighting`). Replace calls to old list functions with new common-cxx equivalents.

### Step 4: Update `addWeightedValue` Callback to Extract Weight from Value (ip-intelligence-cxx)

**`ipi.c`** — The `addWeightedValue` callback (currently `addValueWithPercentage`) is where weighted values are gathered. This callback leverages the **same infrastructure** as profile groups — no new iteration mechanism is needed.

**Why it works with the existing flow:**

The existing iteration path for gathering property values is:

1. **Profile group path:** `addValuesFromProfileGroup` → iterates `offsetPercentage` records → for each profile calls `addValuesFromProfile(rawWeighting=profileWeight)` → `ProfileIterateValuesForProperty` → callback fires once per Value → weight comes from profile-level `rawWeighting`

2. **Singular profile path:** `addValuesFromSingleProfile(rawWeighting=FULL_RAW_WEIGHTING)` → `addValuesFromProfile` → `ProfileIterateValuesForProperty` → callback fires once per Value → weight = 65535

For a **weighted list property** (`isList=true`) on a singular profile, the profile stores **multiple value indices** for that property (one per weighted value — e.g., 3 indices for "US", "GB", "DE" with different weights). `ProfileIterateValuesForProperty` already iterates ALL of them and calls the callback once per Value record. So if the profile has 3 weighted countries, the callback fires 3 times, producing 3 `WeightedItem` entries in the results list.

The only change in the callback is: **check whether the Value record itself carries a weight** (negative `urlOffsetOrWeight`). If so, use it instead of the profile-level `rawWeighting`.

Updated callback:

```c
static bool addWeightedValue(void* state, Item* item) {
    Item valueItem;
    WeightedItem weightedItem;
    const stateWithWeighting* weightState =
        (stateWithWeighting*)((stateWithException*)state)->state;
    ResultsIpi* results = (ResultsIpi*)weightState->subState;
    Exception* exception = ((stateWithException*)state)->exception;
    const DataSetIpi* dataSet = (DataSetIpi*)results->b.dataSet;
    const Value* value = (Value*)item->data.ptr;

    if (value != NULL && results->values.count < results->values.capacity) {
        PropertyValueType const storedValueType = PropertyGetStoredTypeByIndex(
            dataSet->propertyTypes,
            value->propertyIndex,
            exception);
        if (EXCEPTION_OKAY) {
            // Map weighted types to underlying types for reading from
            // the strings collection (e.g., WeightedString → String).
            // The strings collection stores standard values — the weight
            // is in the Value record, not in the stored data.
            PropertyValueType const readType =
                PropertyValueTypeGetUnderlyingType(storedValueType);

            DataReset(&valueItem.data);
            if (StoredBinaryValueGet(
                dataSet->strings,
                value->nameOffset,
                readType,
                &valueItem,
                exception) != NULL && EXCEPTION_OKAY) {
                weightedItem.item = valueItem;

                // If the Value record carries a masked weight (upper 2 bytes
                // == 0xFF00), extract the weight from the lower 2 bytes.
                // This is the case for weighted list properties
                // (WeightedString, WeightedInt, etc.) on singular profiles.
                // Otherwise use the profile-level weight (profile group case).
                if (ValueIsWeighted(value)) {
                    weightedItem.rawWeighting = ValueGetWeight(value);
                } else {
                    weightedItem.rawWeighting = weightState->rawWeighting;
                }

                WeightedItemListAdd(&results->values, &weightedItem);
            }
        }
    }
    COLLECTION_RELEASE(dataSet->values, item);
    return EXCEPTION_OKAY;
}
```

**Summary of what materializes the weighted list:** The combination of `isList=true` (causing the profile to store multiple value indices for the property) + `ProfileIterateValuesForProperty` (which iterates all of them) + the updated callback (which extracts the weight from each Value record) means the `WeightedItemList` on the results is populated with one `WeightedItem` per weighted value, each carrying the correct weight. No new iteration logic is needed — the existing list property infrastructure does the work.

### Step 5: Map Weighted StoredValueTypes to Underlying Types (common-cxx)

Since the strings collection stores **standard** values (not weighted blobs), we need to map weighted `storedValueType` values to their underlying type when reading from the strings collection. This mapping is used in the `addWeightedValue` callback (Step 4).

**`propertyValueType.h` or `storedBinaryValue.c`** — Add a mapping function:

```c
/**
 * Returns the underlying (non-weighted) stored value type for a given
 * property value type. For weighted types (e.g., WeightedString), returns
 * the base type (e.g., String). For non-weighted types, returns the input
 * unchanged.
 */
EXTERNAL fiftyoneDegreesPropertyValueType
fiftyoneDegreesPropertyValueTypeGetUnderlyingType(
    fiftyoneDegreesPropertyValueType type);
```

Implementation:

```c
fiftyoneDegreesPropertyValueType
fiftyoneDegreesPropertyValueTypeGetUnderlyingType(
    fiftyoneDegreesPropertyValueType type) {
    switch (type) {
    case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_STRING:
        return FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
    case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_INTEGER:
        return FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER;
    case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_DOUBLE:
        return FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DOUBLE;
    // Add more as needed
    default:
        return type;
    }
}
```

### Step 6: C++ ResultsIpi Accessor Updates (ip-intelligence-cxx)

**`ResultsIpi.cpp`** — By the time the C++ accessor runs, the `WeightedItemList` on the results is already fully materialized by the callback (Step 4). It contains one `WeightedItem` per value, each with the correct `rawWeighting` — regardless of whether it came from a profile group or from a weighted list property.

The accessor iterates the `WeightedItemList` and produces the output. The logic is **identical** for both weighting sources:

```cpp
// For each WeightedItem in the results:
const StoredBinaryValue *value =
    (const StoredBinaryValue *)weightedItem->item.data.ptr;

WeightedValue<string> wv;
wv.setValue(string(&value->stringValue.value,
                   value->stringValue.size - 1));
wv.setRawWeight(weightedItem->rawWeighting);
values.push_back(wv);
```

**No special handling needed in the accessor for weighted list properties.** The accessor treats all `WeightedItem` entries uniformly. The distinction between weighting sources is entirely handled in the C callback layer (Step 4).

**Concrete example of what the accessor sees:**

```
Profile group case (Country property, non-weighted, profile group with 2 profiles):
  WeightedItemList = [
    { item → "US" (StoredBinaryValue), rawWeighting = 39321 },   // 60% from profile A
    { item → "GB" (StoredBinaryValue), rawWeighting = 26214 },   // 40% from profile B
  ]

Weighted list case (Country property, WeightedString, singular profile):
  WeightedItemList = [
    { item → "US" (StoredBinaryValue), rawWeighting = 42598 },   // from lower 2 bytes of Value[100].urlOffsetOrWeight (0xFF00A666)
    { item → "GB" (StoredBinaryValue), rawWeighting = 22937 },   // from lower 2 bytes of Value[101].urlOffsetOrWeight (0xFF005999)
    { item → "DE" (StoredBinaryValue), rawWeighting = 465 },     // from lower 2 bytes of Value[102].urlOffsetOrWeight (0xFF0001D1)
  ]
```

Both produce `vector<WeightedValue<string>>` via the same code path.

**In `getValuesInternal` (string formatting):** Weighted list properties naturally produce multiple values. The existing string formatting for list properties (e.g., comma-separated) should work as-is for the individual values. If a weighted-specific format is desired (e.g., `"US:0.65,GB:0.35"`), that can be added as a separate formatter.

### Step 7: Debug/Logging Updates (ip-intelligence-cxx)

**`ipi.c`** — Add `"WeightedString"` (and other weighted types) in the storedValueType name switch (~line 783):

```c
case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_STRING: {
    propTypeText = "WeightedString";
    break;
}
case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_INTEGER: {
    propTypeText = "WeightedInteger";
    break;
}
case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_DOUBLE: {
    propTypeText = "WeightedDouble";
    break;
}
```

### Step 8: SWIG Regeneration (ip-intelligence-cxx)

Run `RebuildSwig` with cmake to regenerate SWIG wrappers for both C# and Java. The existing SWIG templates already export:

```swig
%template(WeightedStringListSwig) std::vector<WeightedValue<std::string>>;
```

No changes to the `.i` file are needed — the C++ `getValuesAsWeightedStringList()` returns `vector<WeightedValue<string>>` regardless of how the weights were populated.

### Step 9: .NET Layer (ip-intelligence-dotnet)

**Repository:** `ip-intelligence-dotnet`

1. **Update submodule pointer** to the new `ip-intelligence-cxx`.
2. **Copy regenerated SWIG output** — The SWIG-generated C# interop code in `FiftyOne.IpIntelligence.Engine.OnPremise` will be updated automatically from Step 8.
3. **No changes to managed wrappers** — The existing wrapper classes (`WeightedStringListSwigWrapper.cs`, etc.) consume SWIG types that are unchanged at the C++ template level.
4. **No changes to `IpDataOnPremise.cs`** — `GetValuesAsWeightedStringList()` already calls through to SWIG correctly.
5. **Rebuild native binaries** — Place rebuilt native libraries in `runtimes/{win-x64|linux-x64|osx-x64|osx-arm64}/native/`.
6. **Version bump** — Bump version in `.csproj` files.

### Step 10: Java Layer (ip-intelligence-java)

**Repository:** `ip-intelligence-java`

1. **Update submodule pointer** to the new `ip-intelligence-cxx`.
2. **Copy regenerated SWIG output** — Java SWIG output in `ip-intelligence.engine.on-premise/src/main/java/fiftyone/ipintelligence/engine/onpremise/interop/swig/`.
3. **No changes to data layer** — Already handles weighted values.
4. **Rebuild JNI native library** — Bundle updated `.so`/`.dylib`/`.dll`.
5. **Version bump** — Update version in `pom.xml`.

### Step 11: Examples Updates

**ip-intelligence-dotnet-examples:**

```csharp
// Example: Reading a weighted string property
var values = results.GetValuesAsWeightedStringList("Country");
if (values.HasValue)
{
    foreach (var wv in values.Value)
    {
        Console.WriteLine($"  {wv.Value}: {wv.Weighting():P1}");
    }
}
```

**ip-intelligence-java-examples:**

```java
// Example: Reading a weighted string property
AspectPropertyValue<List<IWeightedValue<String>>> values =
    data.getValuesAsWeightedStringList("Country");
if (values.hasValue()) {
    for (IWeightedValue<String> wv : values.getValue()) {
        System.out.printf("  %s: %.1f%%%n",
            wv.getValue(), wv.getWeighting() * 100);
    }
}
```

## Data Flow Diagram

```
                         DATA FILE
                    +------------------+
                    |  Values collection |
                    |  (each Value has  |
                    |  urlOffsetOrWeight)|
                    +--------+---------+
                             |
                    +--------v---------+
                    | ProfileIterate   |
  Existing          | ValuesForProperty|
  mechanism         |                  |
                    | iterates ALL     |
                    | Value records    |
                    | for the property |
                    | (list property   |
                    |  = multiple)     |
                    +--------+---------+
                             |
                    +--------v---------+
                    | addWeightedValue |
  Step 4            | (ipi.c callback) |
                    |                  |
                    | For each Value:  |
                    |  if masked:      |
                    |   weight = lower |
                    |   2 bytes of     |
                    |   urlOffsetOr    |
                    |   Weight         |
                    |  else:           |
                    |   weight =       |
                    |   profile weight |
                    |                  |
                    | Load stored data |
                    | from strings coll|
                    | (standard type)  |
                    |                  |
                    | -> WeightedItem  |
                    +--------+---------+
                             |
                    +--------v---------+
  Step 6            |  C++ ResultsIpi  |
                    |  accessors       |
                    |                  |
                    | Uniform handling:|
                    | each WeightedItem|
                    | has value + weight|
                    | regardless of    |
                    | source (profile  |
                    | group vs Value   |
                    | record)          |
                    +--------+---------+
                             |
              +--------------+---------------+
              |              |               |
     +--------v---+  +------v------+  +-----v--------+
     |   SWIG     |  |   SWIG      |  |   C API      |
     |   C# / .NET|  |   Java      |  |  getString   |
     +------------+  +-------------+  +--------------+
```

**Weighting sources (handled uniformly by the callback):**

```
Profile Groups (profile-level weight) — non-weighted storedValueTypes:
  Profile A (60%) --> value "foo" --> WeightedItem("foo", rawWeighting=39321)
  Profile B (40%) --> value "bar" --> WeightedItem("bar", rawWeighting=26214)
  Weight source: profile-level rawWeighting from stateWithWeighting

Weighted Values (value-level weight) — weighted storedValueTypes:
  Single Profile --> Value[100] "US" urlOffsetOrWeight=0xFF00A666 (masked, weight=42598)
                 --> Value[101] "GB" urlOffsetOrWeight=0xFF005999 (masked, weight=22937)
                 --> Value[102] "DE" urlOffsetOrWeight=0xFF0001D1 (masked, weight=465)
  Weight source: lower 2 bytes of urlOffsetOrWeight (after mask check)

Both produce the same output: a list of WeightedItem entries.
The C++ accessor doesn't know or care where the weight came from.
```

## Package Cascade

```
common-cxx (Steps 1-2, 5: Value struct change, WeightedItem types, type mapping)
  +-> ip-intelligence-cxx (Steps 3-4, 6-8: consume types, update callback, C++ accessors, SWIG)
       +-> ip-intelligence-dotnet (Step 9: submodule, SWIG copy, native rebuild, version bump)
       |    +-> ip-intelligence-dotnet-examples (Step 11)
       +-> ip-intelligence-java (Step 10: submodule, SWIG copy, native rebuild, version bump)
            +-> ip-intelligence-java-examples (Step 11)
```

## Memory Management

### Simplified Memory Model

With the new approach, there are **no additional memory allocations** compared to non-weighted values:

- No decorator allocations (`StoredListOfStrings`, `stringItems` array)
- No envelope structures
- Each weighted value is a standard `CollectionItem` pointing to a standard string/int/float in the strings collection
- In memory mode, it's just a pointer into mapped memory as usual
- In file/cache mode, standard allocation and release per item

The `WeightedItemList` allocates its array as before (for both weighted and profile-group values). No change there.

### WeightedItem List Cleanup

`WeightedItemListRelease` (formerly `releaseIpiList`) calls `COLLECTION_RELEASE` on each item. Since there is no decorator, all releases go directly to the strings collection. This is identical to the existing behavior.

## Thread Safety

No changes. The Value struct is read-only (const fields). Reading `urlOffsetOrWeight` and checking its sign is thread-safe. The existing concurrent-read guarantees of collections apply unchanged.

## Backward Compatibility

- **Existing data files** without weighted values: `urlOffsetOrWeight` is either a non-negative URL offset or `-1` ("no URL" sentinel). Neither matches the `0xFF00` mask. `ValueGetUrl` returns the URL (or NULL for `-1`) as before. `ValueIsWeighted` returns false. No behavioral change.
- **New data files** with weighted values: `urlOffsetOrWeight` has the `0xFF00` mask in the upper 2 bytes. `ValueGetUrl` returns NULL. `ValueGetWeight` returns the `uint16_t` weight from the lower 2 bytes.
- The type renames (`ProfilePercentage` → `WeightedItem`, `IpiList` → `WeightedItemList`) are source-breaking at the C level, but these types are internal and absorbed by SWIG regeneration.
- The C++ accessor `getValuesAsWeightedStringList` returns the same `vector<WeightedValue<string>>` type regardless of weight source. No API change for consumers.

## Comparison: Old vs New Approach

| Aspect | Old (compound blob) | New (urlOffsetOrWeight) |
|---|---|---|
| Where weight lives | In strings collection blob | In Value record (masked in urlOffsetOrWeight) |
| Strings collection | Compound blob with sub-references | Standard values only |
| Collection decorator | Required (complex) | Not needed |
| New structs | StoredWeightedStringRaw, StoredListOfStrings | None |
| CollectionKeyType | New WeightedString type needed | No change |
| Memory overhead | Envelope + pointer array per get | Zero additional |
| String builder | Special case needed | No change (standard types) |
| Value types supported | Strings only (initially) | Any type (string, int, double, etc.) |
| Complexity | High | Low |

## Related PRs / Issues

- **Pearl design:** `Pearl/WEIGHTED_VALUE_EXPORT_DESIGN.md` — the export-side design this proposal follows
- **common-metadata:** `ValueTypeEnum.WeightedString = 14` (and WeightedInt=15, WeightedDouble=16, etc.) already exist

## Testing

- **Value struct tests:** Verify `ValueIsWeighted` and `ValueGetWeight` for:
  - Masked weighted values (e.g., `0xFF002666` → `isMasked=true`, weight=9830)
  - `-1` sentinel (`0xFFFFFFFF` → `isMasked=false`, weight=0)
  - Valid URL offsets (e.g., `27` → `isMasked=false`, weight=0)
  - Edge cases: `0xFF000000` (weight=0), `0xFF00FFFF` (weight=65535)
- **ValueGetUrl tests:** Verify returns NULL for masked weighted values, NULL for `-1` sentinel, and correct URL for valid non-negative offsets
- **addWeightedValue callback tests:** Verify weight is extracted from Value record for weighted values, and from profile-level weight for non-weighted values
- **Type mapping tests:** Verify `PropertyValueTypeGetUnderlyingType` correctly maps `WeightedString`→`String`, `WeightedInt`→`Int`, etc.
- **List property iteration:** Verify that a profile with multiple weighted values for one property produces the correct number of `WeightedItem` entries
- **C++ accessor tests:** Verify `getValuesAsWeightedStringList` returns correct values and weights from both weighted and profile-group sources
- **Integration tests:** Using an enterprise `.ipi` file with weighted properties, verify end-to-end: query a weighted property → get weighted list → values and weights match expected data
- **Backward compatibility:** Verify existing non-weighted data files work unchanged

## Files Modified (Summary)

| Repository | File | Change |
|---|---|---|
| **common-cxx** | `value.h` | Rename `urlOffset` → `urlOffsetOrWeight` (int32_t), add `ValueGetWeight`/`ValueIsWeighted`, add `VALUE_IS_MASKED` macro |
| **common-cxx** | `value.c` | Update `ValueGetUrl` to check mask and `-1` sentinel; implement `ValueGetWeight`/`ValueIsWeighted` using mask |
| **common-cxx** | `propertyValueType.h` or `storedBinaryValue.c` | Add `PropertyValueTypeGetUnderlyingType` mapping |
| **common-cxx** | `weightedItem.h` (new) | Add `WeightedItem` and `WeightedItemList` type definitions |
| **common-cxx** | `weightedItem.c` (new) | Add list management functions (init, release, extend, add) |
| ip-intelligence-cxx | `ipi.h` | Remove old type defs, include common-cxx header, update macros/signatures |
| ip-intelligence-cxx | `ipi.c` | Rename internals; update `addWeightedValue` to extract weight from Value record |
| ip-intelligence-cxx | `ipi.c` (debug) | Add `"WeightedString"` etc. in type name switch |
| ip-intelligence-cxx | `ResultsIpi.cpp` | Update type references; no special weighted handling needed |
| ip-intelligence-cxx | `ipi_weighted_results.c` | Update to use renamed `WeightedItem` types |
| ip-intelligence-cxx | `fiftyone.h` | Update `MAP_TYPE` macro |
| ip-intelligence-cxx | SWIG output | Regenerate via `RebuildSwig` |
| ip-intelligence-dotnet | Submodule + SWIG + native binaries | Update, rebuild, version bump |
| ip-intelligence-java | Submodule + SWIG + native binaries | Update, rebuild, version bump |
| ip-intelligence-dotnet-examples | Example files | Add/update weighted property example |
| ip-intelligence-java-examples | Example files | Add/update weighted property example |
