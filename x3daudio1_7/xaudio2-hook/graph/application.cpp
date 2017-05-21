#include "stdafx.h"
#include "application.h"

#include "ISpatializationGlue.h"

#include <memory>
#include "x3daudio-hook/Matrix/MatrixEncodingX3DAudioProxy.h"
#include "x3daudio-hook/Matrix/Sound3DRegistry.h"

std::unique_ptr<ISpatializationGlue> s_glue;


void EnsureInitialized()
{
	if (s_glue == nullptr)
	{
		s_glue = std::make_unique<MatrixEncodingX3DAudioProxy>(&Sound3DRegistry::GetInstance());
	}
}

ISpatializedDataExtractor & getSpatializedDataExtractor()
{
	EnsureInitialized();
	return *s_glue;
}

IX3DAudioProxy & getX3DAudioProxy()
{
	EnsureInitialized();
	return *s_glue;
}
