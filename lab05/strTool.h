/*
 * strTool.h
 *
 *  Created on: Sep 14, 2018
 *      Author: samge
 */

#ifndef STRTOOL_H_
#define STRTOOL_H_

#include <string>
#include <queue>
#include <cstring>

class StringUtil {
public:
	StringUtil();
	virtual ~StringUtil();
	static std::queue<std::string> split(std::string& line, std::string delim);
	static char* getCharFromString(std::string line);
	static bool isInteger(std::string str);

};




#endif /* STRTOOL_H_ */
