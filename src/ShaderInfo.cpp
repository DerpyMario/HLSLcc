
#include "ShaderInfo.h"
#include "internal_includes/debug.h"
#include "internal_includes/tokens.h"
#include "Operand.h"
#include <stdlib.h>
#include <sstream>

SHADER_VARIABLE_TYPE ShaderInfo::GetTextureDataType(uint32_t regNo)
{
	const ResourceBinding* psBinding = 0;
	int found;
	found = GetResourceFromBindingPoint(RGROUP_TEXTURE, regNo, &psBinding);
	ASSERT(found != 0);
	return psBinding->GetDataType();
}

void ShaderInfo::GetConstantBufferFromBindingPoint(const ResourceGroup eGroup, const uint32_t ui32BindPoint, const ConstantBuffer** ppsConstBuf) const
{
	ASSERT(ui32MajorVersion > 3);
	*ppsConstBuf = &psConstantBuffers[aui32ResourceMap[eGroup][ui32BindPoint]];
}

int ShaderInfo::GetResourceFromBindingPoint(const ResourceGroup eGroup, uint32_t const ui32BindPoint, const ResourceBinding** ppsOutBinding) const
{
	size_t i;
	const size_t ui32NumBindings = psResourceBindings.size();
	const ResourceBinding* psBindings = &psResourceBindings[0];

	for (i = 0; i < ui32NumBindings; ++i)
	{
		if (ResourceTypeToResourceGroup(psBindings[i].eType) == eGroup)
		{
			if (ui32BindPoint >= psBindings[i].ui32BindPoint && ui32BindPoint < (psBindings[i].ui32BindPoint + psBindings[i].ui32BindCount))
			{
				*ppsOutBinding = psBindings + i;
				return 1;
			}
		}
	}
	return 0;
}

int ShaderInfo::GetInterfaceVarFromOffset(uint32_t ui32Offset, ShaderVar** ppsShaderVar) const
{
	size_t i;
	const size_t ui32NumVars = psThisPointerConstBuffer->asVars.size();

	for (i = 0; i < ui32NumVars; ++i)
	{
		if (ui32Offset >= psThisPointerConstBuffer->asVars[i].ui32StartOffset &&
			ui32Offset < (psThisPointerConstBuffer->asVars[i].ui32StartOffset + psThisPointerConstBuffer->asVars[i].ui32Size))
		{
			*ppsShaderVar = &psThisPointerConstBuffer->asVars[i];
			return 1;
		}
	}
	return 0;
}

int ShaderInfo::GetInputSignatureFromRegister(const uint32_t ui32Register, const uint32_t ui32Mask, const InOutSignature** ppsOut, bool allowNull /* == false */) const
{
	size_t i;
	const size_t ui32NumVars = psInputSignatures.size();

	for (i = 0; i < ui32NumVars; ++i)
	{
		if ((ui32Register == psInputSignatures[i].ui32Register) && (((~psInputSignatures[i].ui32Mask) & ui32Mask) == 0))
		{
			*ppsOut = &psInputSignatures[i];
			return 1;
		}
	}
	ASSERT(allowNull);
	return 0;
}

int ShaderInfo::GetPatchConstantSignatureFromRegister(const uint32_t ui32Register, const uint32_t ui32Mask, const InOutSignature** ppsOut, bool allowNull /* == false */) const
{
	size_t i;
	const size_t ui32NumVars = psPatchConstantSignatures.size();

	for (i = 0; i < ui32NumVars; ++i)
	{
		if ((ui32Register == psPatchConstantSignatures[i].ui32Register) && (((~psPatchConstantSignatures[i].ui32Mask) & ui32Mask) == 0))
		{
			*ppsOut = &psPatchConstantSignatures[i];
			return 1;
		}
	}

	// There are situations (especially when using dcl_indexrange) where the compiler happily writes outside the actual masks.
	// In those situations just take the last signature that uses that register (it's typically the "highest" one)
	for( i = ui32NumVars - 1; i-- > 0; )
	{
		if (ui32Register == psPatchConstantSignatures[i].ui32Register)
		{
			*ppsOut = &psPatchConstantSignatures[i];
			return 1;
		}
	}

	ASSERT(allowNull);
	return 0;
}

int ShaderInfo::GetOutputSignatureFromRegister(const uint32_t ui32Register,
	const uint32_t ui32CompMask,
	const uint32_t ui32Stream,
	const InOutSignature** ppsOut,
	bool allowNull /* = false */) const
{
	size_t i;
	const size_t ui32NumVars = psOutputSignatures.size();
	ASSERT(ui32CompMask != 0);

	for (i = 0; i < ui32NumVars; ++i)
	{
		if (ui32Register == psOutputSignatures[i].ui32Register &&
			(ui32CompMask & psOutputSignatures[i].ui32Mask) &&
			ui32Stream == psOutputSignatures[i].ui32Stream)
		{
			*ppsOut = &psOutputSignatures[i];
			return 1;
		}
	}
	ASSERT(allowNull);
	return 0;
}

int ShaderInfo::GetOutputSignatureFromSystemValue(SPECIAL_NAME eSystemValueType, uint32_t ui32SemanticIndex, const InOutSignature** ppsOut) const
{
	size_t i;
	const size_t ui32NumVars = psOutputSignatures.size();

	for (i = 0; i < ui32NumVars; ++i)
	{
		if (eSystemValueType == psOutputSignatures[i].eSystemValueType &&
			ui32SemanticIndex == psOutputSignatures[i].ui32SemanticIndex)
		{
			*ppsOut = &psOutputSignatures[i];
			return 1;
		}
	}
	ASSERT(0);
	return 0;
}

uint32_t ShaderInfo::GetCBVarSize(const ShaderVarType* psType, bool matrixAsVectors, bool wholeArraySize)
{
    // Default is regular matrices, vectors and scalars 
    uint32_t size = psType->Columns * psType->Rows * 4;

	// Struct size is calculated from the offset and size of its last member.
    // Need to take into account that members could be arrays.
	if (psType->Class == SVC_STRUCT)
	{
        size = psType->Members.back().Offset + GetCBVarSize(&psType->Members.back(), matrixAsVectors, true);
	}
	// Matrices represented as vec4 arrays have special size calculation 
	else if (matrixAsVectors)
	{
		if (psType->Class == SVC_MATRIX_ROWS)
		{
            size = psType->Rows * 16;
		}
		else if (psType->Class == SVC_MATRIX_COLUMNS)
		{
			size = psType->Columns * 16;
		}
	}

    if (wholeArraySize && psType->Elements > 1)
    { 
        uint32_t paddedSize = ((size + 15) / 16) * 16; // Arrays are padded to float4 size
        size = (psType->Elements - 1) * paddedSize + size; // Except the last element
    }

    return size;
}

static const ShaderVarType* IsOffsetInType(const ShaderVarType* psType,
	uint32_t parentOffset,
	uint32_t offsetToFind,
	bool* isArray,
	std::vector<uint32_t>* arrayIndices,
	int32_t* pi32Rebase,
	uint32_t flags)
{
	uint32_t thisOffset = parentOffset + psType->Offset;
	uint32_t thisSize = ShaderInfo::GetCBVarSize(psType, (flags & HLSLCC_FLAG_TRANSLATE_MATRICES) != 0);
	uint32_t paddedSize = ((thisSize + 15) / 16) * 16;
	uint32_t arraySize = thisSize;

	// Array elements are padded to align on vec4 size, except for the last one
	if (psType->Elements)
		arraySize = (paddedSize * (psType->Elements - 1)) + thisSize;

	if ((offsetToFind >= thisOffset) &&
		offsetToFind < (thisOffset + arraySize))
	{
		*isArray = false;
		if (psType->Class == SVC_STRUCT)
		{
			if (psType->Elements > 1 && arrayIndices != NULL)
				arrayIndices->push_back((offsetToFind - thisOffset) / thisSize);

			// Need to bring offset back to element zero in case of array of structs
			uint32_t offsetInStruct = (offsetToFind - thisOffset) % paddedSize;
			uint32_t m = 0;

			for (m = 0; m < psType->MemberCount; ++m)
			{
				const ShaderVarType* psMember = &psType->Members[m];

				const ShaderVarType* foundType = IsOffsetInType(psMember, thisOffset, thisOffset + offsetInStruct, isArray, arrayIndices, pi32Rebase, flags);
				if (foundType != NULL)
					return foundType;
			}
		}
		// Check for array of scalars or vectors (both take up 16 bytes per element).
		// Matrices are also treated as arrays of vectors.
		else if ((psType->Class == SVC_MATRIX_ROWS || psType->Class == SVC_MATRIX_COLUMNS) || 
			((psType->Class == SVC_SCALAR || psType->Class == SVC_VECTOR) && psType->Elements > 1))
		{
			*isArray = true;
			if (arrayIndices != NULL)
				arrayIndices->push_back((offsetToFind - thisOffset) / 16);
		}
		else if (psType->Class == SVC_VECTOR)
		{
			//Check for vector starting at a non-vec4 offset.

			// cbuffer $Globals
			// {
			//
			//   float angle;                       // Offset:    0 Size:     4
			//   float2 angle2;                     // Offset:    4 Size:     8
			//
			// }

			//cb0[0].x = angle
			//cb0[0].yzyy = angle2.xyxx

			//Rebase angle2 so that .y maps to .x, .z maps to .y

			pi32Rebase[0] = thisOffset % 16;
		}

		return psType;
	}
	return NULL;
}

int ShaderInfo::GetShaderVarFromOffset(const uint32_t ui32Vec4Offset,
	const uint32_t(&pui32Swizzle)[4],
	const ConstantBuffer* psCBuf,
	const ShaderVarType** ppsShaderVar, // Output the found var
	bool* isArray, // Output bool that tells if the found var is an array
	std::vector<uint32_t>* arrayIndices, // Output vector of array indices in order from root parent to the found var
	int32_t* pi32Rebase, // Output swizzle rebase
	uint32_t flags)
{
	size_t i;

	uint32_t ui32ByteOffset = ui32Vec4Offset * 16;

	//Swizzle can point to another variable. In the example below
	//cbUIUpdates.g_uMaxFaces would be cb1[2].z. The scalars are combined
	//into vectors. psCBuf->ui32NumVars will be 3.

	// cbuffer cbUIUpdates
	// {
	//   float g_fLifeSpan;                 // Offset:    0 Size:     4
	//   float g_fLifeSpanVar;              // Offset:    4 Size:     4 [unused]
	//   float g_fRadiusMin;                // Offset:    8 Size:     4 [unused]
	//   float g_fRadiusMax;                // Offset:   12 Size:     4 [unused]
	//   float g_fGrowTime;                 // Offset:   16 Size:     4 [unused]
	//   float g_fStepSize;                 // Offset:   20 Size:     4
	//   float g_fTurnRate;                 // Offset:   24 Size:     4
	//   float g_fTurnSpeed;                // Offset:   28 Size:     4 [unused]
	//   float g_fLeafRate;                 // Offset:   32 Size:     4
	//   float g_fShrinkTime;               // Offset:   36 Size:     4 [unused]
	//   uint g_uMaxFaces;                  // Offset:   40 Size:     4
	// }
	if (pui32Swizzle[0] == OPERAND_4_COMPONENT_Y)
	{
		ui32ByteOffset += 4;
	}
	else if (pui32Swizzle[0] == OPERAND_4_COMPONENT_Z)
	{
		ui32ByteOffset += 8;
	}
	else if (pui32Swizzle[0] == OPERAND_4_COMPONENT_W)
	{
		ui32ByteOffset += 12;
	}

	const size_t ui32NumVars = psCBuf->asVars.size();

	for (i = 0; i < ui32NumVars; ++i)
	{
		ppsShaderVar[0] = IsOffsetInType(&psCBuf->asVars[i].sType, psCBuf->asVars[i].ui32StartOffset, ui32ByteOffset, isArray, arrayIndices, pi32Rebase, flags);
		
		if (ppsShaderVar[0] != NULL)
			return 1;
	}
	return 0;
}

// Patches the fullName of the var with given array indices. Does not insert the indexing for the var itself if it is an array.
// Searches for brackets and inserts indices one by one.
std::string ShaderInfo::GetShaderVarIndexedFullName(const ShaderVarType* psShaderVar, const std::vector<uint32_t>& indices, const std::string& dynamicIndex, bool revertDynamicIndexCalc, bool matrixAsVectors)
{
	std::ostringstream oss;
	size_t prevpos = 0;
	size_t pos = psShaderVar->fullName.find('[', 0);
	uint32_t i = 0;
	while (pos != std::string::npos)
	{
		pos++;
		oss << psShaderVar->fullName.substr(prevpos, pos - prevpos);

        // Add possibly given dynamic index for the root array.
        if (i == 0 && !dynamicIndex.empty())
        {
            oss << dynamicIndex;

            // if we couldn't use original index temp, revert the float4 address calc here
            if (revertDynamicIndexCalc)
            {
                const ShaderVarType* psRootVar = psShaderVar;
                while (psRootVar->Parent != NULL)
                    psRootVar = psRootVar->Parent;

                uint32_t thisSize = (GetCBVarSize(psRootVar, matrixAsVectors) + 15) / 16; // size in float4
                oss << " / " << thisSize;
            }

            if (!indices.empty() && indices[i] != 0)
                oss << " + " << indices[i];
        }
		else if (i < indices.size())
			oss << indices[i];

		prevpos = pos;
		i++;
		pos = psShaderVar->fullName.find('[', prevpos);
	}
	oss << psShaderVar->fullName.substr(prevpos);

	return oss.str();
}

ResourceGroup ShaderInfo::ResourceTypeToResourceGroup(ResourceType eType)
{
	switch (eType)
	{
	case RTYPE_CBUFFER:
		return RGROUP_CBUFFER;

	case RTYPE_SAMPLER:
		return RGROUP_SAMPLER;

	case RTYPE_TEXTURE:
	case RTYPE_BYTEADDRESS:
	case RTYPE_STRUCTURED:
		return RGROUP_TEXTURE;

	case RTYPE_UAV_RWTYPED:
	case RTYPE_UAV_RWSTRUCTURED:
	case RTYPE_UAV_RWBYTEADDRESS:
	case RTYPE_UAV_APPEND_STRUCTURED:
	case RTYPE_UAV_CONSUME_STRUCTURED:
	case RTYPE_UAV_RWSTRUCTURED_WITH_COUNTER:
		return RGROUP_UAV;

	case RTYPE_TBUFFER:
		ASSERT(0); // Need to find out which group this belongs to
		return RGROUP_TEXTURE;
    default:
            break;
	}

	ASSERT(0);
	return RGROUP_CBUFFER;
}

void ShaderInfo::AddSamplerPrecisions(HLSLccSamplerPrecisionInfo &info)
{
	if (info.empty())
		return;

	for (size_t i = 0; i < psResourceBindings.size(); i++)
	{
		ResourceBinding *rb = &psResourceBindings[i];
		if (rb->eType != RTYPE_SAMPLER && rb->eType != RTYPE_TEXTURE)
			continue;

		HLSLccSamplerPrecisionInfo::iterator j = info.find(rb->name); // Try finding exact match

		// If match not found, check if name has "sampler" prefix
		// -> try finding a match without the prefix (DX11 style sampler case)
		if (j == info.end() && rb->name.compare(0, 7, "sampler") == 0)
			j = info.find(rb->name.substr(7, rb->name.size() - 7));

		if (j != info.end())
			rb->ePrecision = j->second;
	}
}
