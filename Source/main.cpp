#include "parse.h"
#include "UserType.h"
#include <fstream>
#include <map>
#include <iostream>
#include <algorithm>
#include <sstream>
#include "Type.h"

std::map<std::string, Type*> types;
std::map<std::string, size_t> labelNames;

int main(int argCount, char** args) {
    if (argCount < 4) {
        puts("Not enough args");
        return 1;
    }
    std::string userTypesPath(args[1]);
    std::string stageInputPath(args[2]);
    std::string stageOutputPath(args[3]);

    bool compileToCpp = (stageOutputPath[stageOutputPath.size() - 1] == 'p');
    if (compileToCpp && argCount < 5) {
        puts("Not enough args");
    }

    std::ifstream userTypesFile(userTypesPath);
    std::ifstream stageInputFile(stageInputPath);
    std::ofstream stageOutputFile(stageOutputPath);

    std::vector<std::vector<std::string>> userTypesLines(ParseCsv(userTypesFile));

    std::map<std::string, UserType*> userTypes;
    size_t i = 1;
    for (auto line : userTypesLines) {
        UserType* userType = new UserType(i, userTypes, line);
        userTypes[userType->name] = userType;
        i++;
    }

    std::vector<std::vector<std::string>> stageLines(ParseCsv(stageInputFile));
    DataStream stageOutputStream;

    size_t index = 0;
    for (auto line : stageLines) {
        std::string name(line.at(0));
        std::string userTypeName;
        size_t colonPos = 0;
        for (size_t i = 0; i < name.size(); i++) {
            char ch = name[i];
            if (ch == ':') {
                name[i] = '\0';
                userTypeName = &name[0];
                name[i] = ':';
                colonPos = i + 1;
            } else if (ch == ' ') {
                colonPos++;
            }
        }
        if (colonPos != 0) {
            std::string labelName(&name[colonPos]);
            labelNames[labelName] = index;
        } else {
            userTypeName = name;
        }
        if (!userTypes.count(userTypeName)) {
            std::cout << "Invalid user type " << userTypeName << "\n";
            return 1;
        }
        UserType* userType = userTypes[userTypeName];

        stageOutputStream << (ushort)userType->id;

        struct PropertyValue {
            Property* property;
            std::string value;
        };
        std::vector<PropertyValue> propertyValues;

        for (size_t i = 1; i < line.size(); i += 2) {
            std::string propertyName(line[i]);
            if (propertyName.size()) {
                Property* property = userType->GetProperty(propertyName);
                if (property == nullptr) {
                    std::cout << "Invalid property " << propertyName << "\n";
                    return 1;
                }

                std::string value(line.at(i + 1));
                propertyValues.push_back({ property, value });
            }
        }

        std::sort(propertyValues.begin(), propertyValues.end(), [](const PropertyValue& a, const PropertyValue& b) {
            return a.property->id < b.property->id;
        });
        
        for (size_t i = 0; i < propertyValues.size(); i++) {
            const PropertyValue& propertyValue = propertyValues[i];
            if (propertyValue.property->id != i) {
                std::cout << "Missing property with id " << propertyValue.property->id << " and value " << propertyValue.value << "\n";
                return 1;
            }
            propertyValue.property->type->Save(stageOutputStream, propertyValue.value);
        }
        index++;
    }

    if (compileToCpp) {
        std::string stageName(args[4]);
        std::stringstream outputStringStream;
        outputStringStream << "// generated by scc DO NOT EDIT\n#include \"stageLinks.h\"\n#include \"Wii/type.h\"\nuchar buf[] = { ";
        for (size_t i = 0; i < stageOutputStream.size; i++) {
            u_char ch = ((u_char*)(stageOutputStream.buf))[i];
            outputStringStream << (size_t)ch << ", ";
        }
        outputStringStream << " };\nStageData " << stageName << " = { buf, " << stageOutputStream.size << " };\n";
        std::string outputString(outputStringStream.str());
        stageOutputFile.write(outputString.data(), outputString.size());
    } else {
        stageOutputFile.write((const char*)(stageOutputStream.buf), stageOutputStream.size);
    }
}