/*
 * strTool.cpp
 *
 *  Created on: Sep 14, 2018
 *      Author: samge
 */

#include "strTool.h"

StringUtil::StringUtil() {

}

StringUtil::~StringUtil() {
}

// Taken from: https://stackoverflow.com/a/37454181
std::queue<std::string> StringUtil::split(std::string& line, std::string delim) {
	std::queue<std::string> output;
	size_t prev = 0, pos = 0;

	do {
		pos = line.find(delim, prev);
		if(pos == std::string::npos) {
			pos = line.size();
		}

		output.push(line.substr(prev, pos - prev));
		prev = pos + delim.length();
	} while(pos < line.length() && prev < line.length());

	return output;
}

// Taken from: https://stackoverflow.com/a/4207749
char* StringUtil::getCharFromString(std::string line) {
	char *str = new char[line.size()+1]; // +1 to account for \0 byte
	std::strncpy(str, line.c_str(), line.size());
	return str;
}

bool StringUtil::isInteger(std::string str) {
	for(unsigned int i = 0; i < str.length(); i ++) {
		if(str[i] < '0' || str[i] > '9') {
			return false;
		}
	}

	return true;
}


