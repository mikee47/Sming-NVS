
#pragma once

#include <TestBase.h>

class NvsTestBase : public TestBase
{
public:
	bool testVerify(bool res, const TestParam& param) override;
};
