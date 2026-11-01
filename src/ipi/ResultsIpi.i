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

%include stdint.i
%include std_pair.i
%include std_vector.i

%extend std::vector<uint8_t> {
    int copyToArray(unsigned char copy[], int length) {
        if (copy == nullptr || length <= 0) {
            return 0;
        }
        int count = length < (int)$self->size() ? length : (int)$self->size();
        for (int i = 0; i < count; i++) {
            copy[i] = (*$self)[i];
        }
        return count;
    }
}
%include "../common/ResultsBase.i"
%include "WeightedValue.i"
%include "../common/IpAddress.i"

%template(WeightedStringListSwig) std::vector<WeightedValue<std::string>>;
%template(WeightedUTF8StringListSwig) std::vector<WeightedValue<std::vector<uint8_t>>>;
%template(WeightedBoolListSwig) std::vector<WeightedValue<bool>>;
%template(WeightedIntListSwig) std::vector<WeightedValue<int>>;
%template(WeightedDoubleListSwig) std::vector<WeightedValue<double>>;

%template(WeightedStringListValueSwig) Value<std::vector<WeightedValue<std::string>>>;
%template(WeightedUTF8StringListValueSwig) Value<std::vector<WeightedValue<std::vector<uint8_t>>>>;
%template(WeightedBoolListValueSwig) Value<std::vector<WeightedValue<bool>>>;
%template(WeightedIntListValueSwig) Value<std::vector<WeightedValue<int>>>;
%template(WeightedDoubleListValueSwig) Value<std::vector<WeightedValue<double>>>;
%template(IpAddressValueSwig) Value<IpAddress>;
%template(UTF8StringValueSwig) Value<std::vector<uint8_t>>;

%nodefaultctor ResultsIpi;

%rename (ResultsIpiSwig) ResultsIpi;

class ResultsIpi : public ResultsBase {
public:
    virtual ~ResultsIpi();

    Value<std::vector<WeightedValue<std::string>>> getValuesAsWeightedStringList(
        const std::string &propertyName);
    Value<std::vector<WeightedValue<std::string>>> getValuesAsWeightedStringList(
        int requiredPropertyIndex);

    Value<std::vector<WeightedValue<std::vector<uint8_t>>>> getValuesAsWeightedUTF8StringList(
        const std::string &propertyName);
    Value<std::vector<WeightedValue<std::vector<uint8_t>>>> getValuesAsWeightedUTF8StringList(
        int requiredPropertyIndex);

    Value<std::vector<uint8_t>> getValueAsUTF8String(
        const std::string &propertyName);
    Value<std::vector<uint8_t>> getValueAsUTF8String(
        int requiredPropertyIndex);

    Value<std::vector<WeightedValue<std::string>>> getValuesAsWeightedWKTStringList(
        const std::string &propertyName, uint8_t decimalPlaces);
    Value<std::vector<WeightedValue<std::string>>> getValuesAsWeightedWKTStringList(
        int requiredPropertyIndex, uint8_t decimalPlaces);

    Value<std::vector<WeightedValue<bool>>> getValuesAsWeightedBoolList(
        const std::string &propertyName);
    Value<std::vector<WeightedValue<bool>>> getValuesAsWeightedBoolList(
        int requiredPropertyIndex);
    
    Value<std::vector<WeightedValue<int>>> getValuesAsWeightedIntegerList(
        const std::string &propertyName);
    Value<std::vector<WeightedValue<int>>> getValuesAsWeightedIntegerList(
        int requiredPropertyIndex);

    Value<std::vector<WeightedValue<double>>> getValuesAsWeightedDoubleList(
        const std::string &propertyName);
    Value<std::vector<WeightedValue<double>>> getValuesAsWeightedDoubleList(
        int requiredPropertyIndex);

    Value<IpAddress> getValueAsIpAddress(
        const std::string &propertyName);
    Value<IpAddress> getValueAsIpAddress(
        int requiredPropertyIndex);

    Value<IpAddress> getTargetIpAddress();
};

%extend ResultsIpi {
    int copyValueAsUTF8String(
        int requiredPropertyIndex,
        unsigned char copy[],
        int length) {
        Value<std::vector<uint8_t>> value =
            $self->getValueAsUTF8String(requiredPropertyIndex);
        if (!value.hasValue()) {
            return -1;
        }
        std::vector<uint8_t> bytes = value.getValue();
        int required = (int)bytes.size();
        if (copy != nullptr && length >= required) {
            for (int i = 0; i < required; i++) {
                copy[i] = bytes[i];
            }
        }
        return required;
    }

    int getRequiredPropertyIndex(
        unsigned char propertyName[],
        int propertyNameLength) {
        if (propertyName == nullptr || propertyNameLength < 0) {
            return -1;
        }
        return $self->getRequiredPropertyIndex(
            (const char *)propertyName,
            (size_t)propertyNameLength);
    }

    // Returns the number of bytes the value needs, -1 if the property has no
    // value, or -2 if it is not a required property of these results.
    int copyValueAsUTF8StringByName(
        unsigned char propertyName[],
        int propertyNameLength,
        unsigned char copy[],
        int length) {
        if (propertyName == nullptr || propertyNameLength < 0) {
            return -2;
        }
        const int index = $self->getRequiredPropertyIndex(
            (const char *)propertyName,
            (size_t)propertyNameLength);
        if (index < 0) {
            return -2;
        }
        Value<std::vector<uint8_t>> value =
            $self->getValueAsUTF8String(index);
        if (!value.hasValue()) {
            return -1;
        }
        std::vector<uint8_t> bytes = value.getValue();
        int required = (int)bytes.size();
        if (copy != nullptr && length >= required) {
            for (int i = 0; i < required; i++) {
                copy[i] = bytes[i];
            }
        }
        return required;
    }
}
