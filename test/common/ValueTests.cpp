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
 
#include "pch.h"
#include "Base.hpp"
#include "../../src/common/Value.hpp"
#include "../../src/common/ResultsBase.hpp"
#include "../../src/common/dataset.h"

using std::vector;
using namespace FiftyoneDegrees::Common;

/**
 * Value test class used to test the functionality of Value.cpp.
 */
class ValueTests : public Base {
protected:

	/**
	 * Calls the base setup method to enable memory leak checking and memory
	 * allocation checking.
	 */
	void SetUp() {
		Base::SetUp();
	}

	/**
	 * Calls the base teardown method to check for memory leaks and compare
	 * expected and actual memory allocations.
	 */
	void TearDown() {
		Base::TearDown();
	}
};

/**
 * Check that the hasValue() method returns false when a value has not yet been
 * set, and true once it has.
 */
TEST_F(ValueTests, HasValue) {
	Value<string> value;

	EXPECT_FALSE(value.hasValue()) <<
		L"The value has not been set, so hasValue() should have returned false.";
	value.setValue("some value");
	EXPECT_TRUE(value.hasValue()) <<
		L"The value has been set, so hasValue() should have returned true.";
}

/**
 * Check that the getValue() method returns the value which has been set by the
 * setValue() method.
 */
TEST_F(ValueTests, GetValue) {
	int expected = 51;
	Value<int> value;

	value.setValue(expected);

	EXPECT_EQ(expected, value.getValue()) <<
		L"The value returned was not the value which was set.";
}

/**
 * Check that the * operator returns the value which has been set by the
 * setValue() method.
 */
TEST_F(ValueTests, GetValueWithOperator) {
	int expected = 51;
	Value<int> value;

	value.setValue(expected);

	EXPECT_EQ(expected, *value) << 
		L"The value returned was not that value which was set.";
}

/**
 * Check that an exception is thrown when an attempt is made to get a value
 * using the getValue() method when it has not been set.
 */
TEST_F(ValueTests, GetUnsetValue) {
	Value<int> value;
	ASSERT_THROW(value.getValue(), NoValuesAvailableException) <<
		L"No exception was thrown, even though no value was there to be returned.";
}

/**
 * Check that an exception is thrown when an attempt is made to get a value
 * using the * operator when it has not been set.
 */
TEST_F(ValueTests, GetUnsetValueWithOperator) {
	Value<int> value;
	ASSERT_THROW(*value, NoValuesAvailableException) <<
		L"No exception was thrown, even though no value was there to be returned.";
}

/**
 * Check that when the no value reason is set to "invalid property", that an
 * InvalidPropertyException is thrown when attempting to get the value.
 */
TEST_F(ValueTests, GetInvalidProperty) {
	Value<int> value;
	value.setNoValueReason(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
		nullptr);
	ASSERT_THROW(*value, InvalidPropertyException) <<
		L"The wrong exception was thrown for an invalid property.";
}

/**
 * Check that when the no value reason is set to "too many properties", that an
 * TooManyValuesException is thrown when attempting to get the value.
 */
TEST_F(ValueTests, GetTooManyValues) {
	Value<int> value;
	value.setNoValueReason(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_TOO_MANY_VALUES,
		nullptr);
	ASSERT_THROW(*value, TooManyValuesException) <<
		L"The wrong exception was thrown for too many values.";
}

/**
 * Check that when a no value message is set using the setNoValueException()
 * method, the same message is returned by the getNoValueMessage() method.
 */
TEST_F(ValueTests, GetNoValueMessage) {
	const char *expected = "some error message";
	Value<int> value;

	value.setNoValueReason(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS,
		expected);

	ASSERT_STREQ(expected, value.getNoValueMessage()) <<
		L"The no value message returned was not the one which was set.";
}

/**
 * Check that when a no value message is set using the setNoValueException()
 * method, the same message is used in the exception thrown.
 */
TEST_F(ValueTests, ExceptionMessage) {
	const char *expected = "some error message";
	Value<int> value;

	value.setNoValueReason(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS,
		expected);

	try {
		value.getValue();
		FAIL() << L"This point should not be reached. See GetUnsetValue test.";
	}
	catch (NoValuesAvailableException& e) {
		ASSERT_STREQ(expected, e.what()) <<
			L"The no value message in the exception was not the one which was set.";
	}
}

/**
 * Check that behavior when using a complex type is the same.
 */
TEST_F(ValueTests, ComplexType) {
	vector<string> expected;
	Value<vector<string>> value;

	value.setValue(expected);

	ASSERT_EQ(expected, *value) <<
		L"The value returned was not the same as the one which was set.";
}

/**
 * Check that behavior when using a pointer to a type is the same.
 */
TEST_F(ValueTests, PointerType) {
	vector<string> *expected = new vector<string>();
	Value<vector<string>*> value;

	value.setValue(expected);

	ASSERT_EQ(expected, *value) <<
		L"The value returned was not the same as the one which was set.";

	delete expected;
}

/**
 * Check that every read-only method can be called on a const Value. This guards
 * against a regression of the reported issue where the getters were missing the
 * const qualifier, so code such as
 *     Value<string> const v = results->getValueAsString(index);
 *     if (v.hasValue()) { ... }
 * failed to compile. This test will not compile if any read method loses its
 * const qualifier.
 */
TEST_F(ValueTests, ConstAccessWithValue) {
	int expected = 51;
	Value<int> value;
	value.setValue(expected);

	const Value<int> &constValue = value;

	EXPECT_TRUE(constValue.hasValue()) <<
		L"hasValue() should be callable on a const Value and return true.";
	EXPECT_EQ(expected, constValue.getValue()) <<
		L"getValue() should be callable on a const Value and return the value.";
	EXPECT_EQ(expected, *constValue) <<
		L"operator*() should be callable on a const Value and return the value.";
}

/**
 * Check that the no-value read methods are also callable on a const Value, so
 * the const path can inspect why a value is missing.
 */
TEST_F(ValueTests, ConstAccessNoValue) {
	const char *expected = "some error message";
	Value<int> value;
	value.setNoValueReason(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS,
		expected);

	const Value<int> &constValue = value;

	EXPECT_FALSE(constValue.hasValue()) <<
		L"hasValue() should be callable on a const Value and return false.";
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS,
		constValue.getNoValueReason()) <<
		L"getNoValueReason() should be callable on a const Value.";
	EXPECT_STREQ(expected, constValue.getNoValueMessage()) <<
		L"getNoValueMessage() should be callable on a const Value.";
}

/**
 * Results with values set by the test, so the typed readers of ResultsBase
 * can be tested without a data file. The message is the same for every
 * reason, so a test can tell it came from getNoValueMessageInternal.
 */
class FakeResults : public ResultsBase {
public:
	static constexpr const char *Message = "message from the engine";

	FakeResults(
		fiftyoneDegreesResultsBase *base,
		const vector<string> &values) :
		ResultsBase(base, shared_ptr<fiftyoneDegreesResourceManager>()),
		values(values) {
	}

protected:
	void getValuesInternal(int, vector<string> &out) override {
		out = values;
	}
	bool hasValuesInternal(int) override {
		return true;
	}
	const char* getNoValueMessageInternal(
		fiftyoneDegreesResultsNoValueReason) override {
		return Message;
	}
	fiftyoneDegreesResultsNoValueReason getNoValueReasonInternal(
		int) override {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_UNKNOWN;
	}

private:
	vector<string> values;
};

/**
 * Tests the typed readers of ResultsBase for a property which has values
 * according to hasValuesInternal, but where the values are an empty or a
 * multiple list.
 */
class ResultsBaseNoValueTests : public Base {
protected:
	void SetUp() {
		Base::SetUp();
		memset(dataSetBytes, 0, sizeof(dataSetBytes));
		memset(&available, 0, sizeof(available));
		memset(&named, 0, sizeof(named));
		memset(&base, 0, sizeof(base));
		available.named = &named;
		reinterpret_cast<fiftyoneDegreesDataSetBase*>(dataSetBytes)
			->available = &available;
		base.dataSet = dataSetBytes;
	}

	void TearDown() {
		Base::TearDown();
	}

	template <class T> void expectNoValue(
		const Value<T> &value,
		fiftyoneDegreesResultsNoValueReason reason,
		const char *reader) {
		EXPECT_FALSE(value.hasValue()) << reader;
		EXPECT_EQ(reason, value.getNoValueReason()) << reader;
		ASSERT_NE(nullptr, value.getNoValueMessage()) << reader;
		EXPECT_STREQ(FakeResults::Message, value.getNoValueMessage())
			<< reader;
	}

	// The data set holds const members, so it is a zeroed block of bytes.
	alignas(fiftyoneDegreesDataSetBase)
		unsigned char dataSetBytes[sizeof(fiftyoneDegreesDataSetBase)];
	fiftyoneDegreesPropertiesAvailable available;
	fiftyoneDegreesPropertyNamed named;
	fiftyoneDegreesResultsBase base;
};

/**
 * A property whose values list is empty must give the no result for
 * property reason and a message from every typed reader, because the JSON
 * Builder writes the message into the nullreason property.
 */
TEST_F(ResultsBaseNoValueTests, EmptyValuesGiveNoResultForPropertyMessage) {
	FakeResults results(&base, vector<string>());
	const fiftyoneDegreesResultsNoValueReason reason =
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULT_FOR_PROPERTY;

	expectNoValue(results.getValueAsString(0), reason, "string");
	expectNoValue(results.getValueAsBool(0), reason, "bool");
	expectNoValue(results.getValueAsInteger(0), reason, "integer");
	expectNoValue(results.getValueAsDouble(0), reason, "double");
	expectNoValue(results.getValueAsStrictBool(0), reason, "strict bool");
	expectNoValue(
		results.getValueAsStrictInteger(0), reason, "strict integer");
	expectNoValue(
		results.getValueAsStrictDouble(0), reason, "strict double");
}

/**
 * A property with more than one value cannot be returned as one bool,
 * integer or double, and the reason must carry a message.
 */
TEST_F(ResultsBaseNoValueTests, ManyValuesGiveTooManyValuesMessage) {
	FakeResults results(&base, vector<string>({ "True", "False" }));
	const fiftyoneDegreesResultsNoValueReason reason =
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_TOO_MANY_VALUES;

	expectNoValue(results.getValueAsBool(0), reason, "bool");
	expectNoValue(results.getValueAsInteger(0), reason, "integer");
	expectNoValue(results.getValueAsDouble(0), reason, "double");
	expectNoValue(results.getValueAsStrictBool(0), reason, "strict bool");
	expectNoValue(
		results.getValueAsStrictInteger(0), reason, "strict integer");
	expectNoValue(
		results.getValueAsStrictDouble(0), reason, "strict double");
}
