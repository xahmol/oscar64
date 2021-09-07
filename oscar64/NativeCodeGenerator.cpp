#include "NativeCodeGenerator.h"

NativeCodeInstruction::NativeCodeInstruction(AsmInsType type, AsmInsMode mode, int address, int varIndex, bool lower, bool upper)
	: mType(type), mMode(mode), mAddress(address), mVarIndex(varIndex), mLower(lower), mUpper(upper)
{}


void NativeCodeInstruction::Assemble(NativeCodeBasicBlock* block)
{
	block->PutByte(AsmInsOpcodes[mType][mMode]);

	switch (mMode)
	{
	case ASMIM_IMPLIED:
		break;
	case ASMIM_ZERO_PAGE:
	case ASMIM_ZERO_PAGE_X:
	case ASMIM_INDIRECT_X:
	case ASMIM_INDIRECT_Y:
		block->PutByte(uint8(mAddress));
		break;
	case ASMIM_IMMEDIATE:
		if (mVarIndex != -1)
		{
			ByteCodeRelocation		rl;
			rl.mAddr = block->mCode.Size();
			rl.mFunction = false;
			rl.mLower = mLower;
			rl.mUpper = mUpper;
			rl.mIndex = mVarIndex;
			rl.mOffset = mAddress;
			block->mRelocations.Push(rl);
			block->PutByte(0);
		}
		else
		{
			block->PutByte(uint16(mAddress));
		}
		break;
	case ASMIM_ABSOLUTE:
	case ASMIM_INDIRECT:
	case ASMIM_ABSOLUTE_X:
	case ASMIM_ABSOLUTE_Y:
		if (mVarIndex != - 1)
		{
			ByteCodeRelocation		rl;
			rl.mAddr = block->mCode.Size();
			rl.mFunction = false;
			rl.mLower = true;
			rl.mUpper = true;
			rl.mIndex = mVarIndex;
			rl.mOffset = mAddress;
			block->mRelocations.Push(rl);
			block->PutWord(0);
		}
		else
		{
			block->PutWord(uint16(mAddress));
		}
		break;
	case ASMIM_RELATIVE:
		block->PutByte(uint8(mAddress));
		break;
	}
}


void NativeCodeBasicBlock::PutByte(uint8 code)
{
	this->mCode.Insert(code);
}

void NativeCodeBasicBlock::PutWord(uint16 code)
{
	this->mCode.Insert((uint8)(code & 0xff));
	this->mCode.Insert((uint8)(code >> 8));
}

static AsmInsType InvertBranchCondition(AsmInsType code)
{
	switch (code)
	{
	case ASMIT_BEQ: return ASMIT_BNE;
	case ASMIT_BNE: return ASMIT_BEQ;
	case ASMIT_BPL: return ASMIT_BMI;
	case ASMIT_BMI: return ASMIT_BPL;
	case ASMIT_BCS: return ASMIT_BCC;
	case ASMIT_BCC: return ASMIT_BCS;
	default:
		return code;
	}
}

static AsmInsType TransposeBranchCondition(AsmInsType code)
{
	switch (code)
	{
	case ASMIT_BEQ: return ASMIT_BEQ;
	case ASMIT_BNE: return ASMIT_BNE;
	case ASMIT_BPL: return ASMIT_BMI;
	case ASMIT_BMI: return ASMIT_BPL;
	case ASMIT_BCS: return ASMIT_BCC;
	case ASMIT_BCC: return ASMIT_BCS;
	default:
		return code;
	}
}


int NativeCodeBasicBlock::PutJump(NativeCodeProcedure* proc, int offset)
{
	PutByte(0x4c);

	ByteCodeRelocation		rl;
	rl.mAddr = mCode.Size();
	rl.mFunction = true;
	rl.mLower = true;
	rl.mUpper = true;
	rl.mIndex = proc->mIndex;
	rl.mOffset = mOffset + mCode.Size() + offset - 1;
	mRelocations.Push(rl);

	PutWord(0);
	return 3;
}

int NativeCodeBasicBlock::PutBranch(NativeCodeProcedure* proc, AsmInsType code, int offset)
{
	if (offset >= -126 && offset <= 129)
	{
		PutByte(AsmInsOpcodes[code][ASMIM_RELATIVE]);
		PutByte(offset - 2);
		return 2;
	}
	else
	{
		PutByte(AsmInsOpcodes[InvertBranchCondition(code)][ASMIM_RELATIVE]);
		PutByte(3);
		PutByte(0x4c);

		ByteCodeRelocation	rl;
		rl.mAddr = mCode.Size();
		rl.mFunction = true;
		rl.mLower = true;
		rl.mUpper = true;
		rl.mIndex = proc->mIndex;
		rl.mOffset = mOffset + mCode.Size() + offset - 3;
		mRelocations.Push(rl);
		
		PutWord(0);
		return 5;
	}
}

void NativeCodeBasicBlock::LoadConstant(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.mTType == IT_FLOAT)
	{
	}
	else if (ins.mTType == IT_POINTER)
	{
		if (ins.mMemory == IM_GLOBAL)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue, ins.mVarIndex, true, false));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue, ins.mVarIndex, false, true));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else if (ins.mMemory == IM_ABSOLUTE)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mIntValue >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
		{
			int	index = ins.mIntValue;
			if (ins.mMemory == IM_LOCAL)
				index += proc->mLocalVars[ins.mVarIndex].mOffset;
			else
				index += ins.mVarIndex + proc->mLocalSize + 2;

			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, index & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, (mNoFrame ? BC_REG_STACK : BC_REG_LOCALS) + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (index >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else if (ins.mMemory == IM_PROCEDURE)
		{
		}
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mIntValue >> 8) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
	}

}

void NativeCodeBasicBlock::StoreValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.mSType[0] == IT_FLOAT)
	{
	}
	else if (ins.mSType[0] == IT_POINTER)
	{
		if (ins.mSTemp[1] < 0)
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[1];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
				else if (ins.mMemory == IM_FRAME)
				{
				}
			}
			else
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[1];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
				else if (ins.mMemory == IM_FRAME)
				{
				}
			}
		}
		else
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				}
			}
			else
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				}
			}
		}
	}
	else
	{
		if (ins.mSTemp[1] < 0)
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mOperandSize == 1)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
				else if (ins.mOperandSize == 2)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
			}
			else
			{
				if (ins.mOperandSize == 1)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
				else if (ins.mOperandSize == 2)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
			}
		}
		else
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));

					if (ins.mOperandSize == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					}
				}
			}
			else
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));

					if (ins.mOperandSize == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					}
				}
			}
		}
	}

}

void NativeCodeBasicBlock::LoadStoreValue(InterCodeProcedure* proc, const InterInstruction& rins, const InterInstruction& wins)
{
	if (rins.mTType == IT_FLOAT)
	{

	}
	else if (rins.mTType == IT_POINTER)
	{

	}
	else
	{
		if (wins.mOperandSize == 1)
		{
			if (rins.mSTemp[0] < 0)
			{
				if (rins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins.mSIntConst[0], rins.mVarIndex));
				}
				else if (rins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins.mSIntConst[0]));
				}
				else if (rins.mMemory == IM_LOCAL || rins.mMemory == IM_PARAM)
				{
					int	index = rins.mSIntConst[0];
					if (rins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[rins.mVarIndex].mOffset;
					else
						index += rins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
			}
			else
			{
				if (rins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[wins.mSTemp[0]]));
				}
			}

			if (wins.mSTemp[1] < 0)
			{
				if (wins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins.mSIntConst[1], wins.mVarIndex));
				}
				else if (wins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins.mSIntConst[1]));
				}
				else if (wins.mMemory == IM_LOCAL || wins.mMemory == IM_PARAM)
				{
					int	index = wins.mSIntConst[1];
					if (wins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[wins.mVarIndex].mOffset;
					else
						index += wins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
				else if (wins.mMemory == IM_FRAME)
				{
				}
			}
			else
			{
				if (wins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[wins.mSTemp[1]]));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::LoadValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.mTType == IT_FLOAT)
	{
	}
	else if (ins.mTType == IT_POINTER)
	{
		if (ins.mSTemp[0] < 0)
		{
			if (ins.mMemory == IM_GLOBAL)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0], ins.mVarIndex));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1, ins.mVarIndex));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
			}
			else if (ins.mMemory == IM_ABSOLUTE)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
			}
			else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
			{
				int	index = ins.mSIntConst[0];
				if (ins.mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins.mVarIndex].mOffset;
				else
					index += ins.mVarIndex + proc->mLocalSize + 2;

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
			}
		}
		else
		{
			if (ins.mMemory == IM_INDIRECT)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
			}
		}
	}
	else
	{
		if (ins.mSTemp[0] < 0)
		{
			if (ins.mOperandSize == 1)
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0], ins.mVarIndex));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0]));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[0];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
				if (ins.mTType == IT_SIGNED)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
					mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
			}
			else if (ins.mOperandSize == 2)
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0], ins.mVarIndex));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1, ins.mVarIndex));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[0];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
			}
		}
		else
		{
			if (ins.mMemory == IM_INDIRECT)
			{
				if (ins.mOperandSize == 1)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					if (ins.mTType == IT_SIGNED)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
						mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
				else if (ins.mOperandSize == 2)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, const InterInstruction& ins)
{
	switch (ins.mOperator)
	{
	case IA_ADD:
	{
		if (ins.mSTemp[0] < 0 && ins.mSIntConst[0] == 1 && ins.mSTemp[1] == ins.mTTemp ||
			ins.mSTemp[1] < 0 && ins.mSIntConst[1] == 1 && ins.mSTemp[0] == ins.mTTemp)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, 2));
			mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			if (ins.mSTemp[1] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
			if (ins.mSTemp[0] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			if (ins.mSTemp[1] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
			if (ins.mSTemp[0] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
	} break;
	case IA_SUB:
	{
		mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
		if (ins.mSTemp[1] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
		if (ins.mSTemp[0] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
		if (ins.mSTemp[1] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
		if (ins.mSTemp[0] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
	} break;
	case IA_SHL:
	{
		if (ins.mSTemp[0] < 0)
		{
			if (ins.mSIntConst[0] == 1)
			{
				if (ins.mSTemp[1] == ins.mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
				}
			}
		}
	} break;
	}
}

void NativeCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, const InterInstruction& ins)
{

}

void NativeCodeBasicBlock::RelationalOperator(InterCodeProcedure* proc, const InterInstruction& ins, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump)
{
	switch (ins.mOperator)
	{
	case IA_CMPEQ:
	{
		if (ins.mSTemp[1] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
		if (ins.mSTemp[0] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));

		NativeCodeBasicBlock* tblock = new NativeCodeBasicBlock();
		tblock->mNoFrame = mNoFrame;
		tblock->mIndex = 1000;

		this->Close(falseJump, tblock, ASMIT_BNE);

		if (ins.mSTemp[1] < 0)
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
		else
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
		if (ins.mSTemp[0] < 0)
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
		else
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));

		tblock->Close(falseJump, trueJump, ASMIT_BNE);
	}	break;
	case IA_CMPNE:
	{
		if (ins.mSTemp[1] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
		if (ins.mSTemp[0] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));

		NativeCodeBasicBlock* tblock = new NativeCodeBasicBlock();
		tblock->mNoFrame = mNoFrame;
		tblock->mIndex = 1000;

		this->Close(trueJump, tblock, ASMIT_BNE);

		if (ins.mSTemp[1] < 0)
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
		else
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
		if (ins.mSTemp[0] < 0)
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
		else
			tblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));

		tblock->Close(trueJump, falseJump, ASMIT_BNE);
	}	break;
	}
}

void NativeCodeBasicBlock::LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction& ins)
{
	mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
	if (ins.mSTemp[1] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
	if (ins.mSTemp[0] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
	mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
	if (ins.mSTemp[1] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
	if (ins.mSTemp[0] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
	mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
}

void NativeCodeBasicBlock::Compile(InterCodeProcedure* iproc, NativeCodeProcedure* proc, InterCodeBasicBlock* sblock)
{
	mIndex = sblock->mIndex;

	int	i = 0;
	while (i < sblock->mInstructions.Size())
	{
		const InterInstruction& ins = sblock->mInstructions[i];

		switch (ins.mCode)
		{
		case IC_STORE:
			StoreValue(iproc, ins);
			break;
		case IC_LOAD:
			if (i + 1 < sblock->mInstructions.Size() && 
				sblock->mInstructions[i + 1].mCode == IC_STORE && 
				sblock->mInstructions[i + 1].mSTemp[0] == ins.mTTemp && 
				sblock->mInstructions[i + 1].mSFinal[0] &&
				sblock->mInstructions[i + 1].mOperandSize == 1)
			{
				LoadStoreValue(iproc, ins, sblock->mInstructions[i + 1]);
				i++;
			}
			else
				LoadValue(iproc, ins);
			break;
		case IC_COPY:
//			CopyValue(iproc, ins);
			break;
		case IC_LOAD_TEMPORARY:
		{
			if (ins.mSTemp[0] != ins.mTTemp)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp] + 1));
			}
		}	break;
		case IC_BINARY_OPERATOR:
			BinaryOperator(iproc, ins);
			break;
		case IC_UNARY_OPERATOR:
			UnaryOperator(iproc, ins);
			break;
		case IC_CONVERSION_OPERATOR:
//			NumericConversion(iproc, ins);
			break;
		case IC_LEA:
			LoadEffectiveAddress(iproc, ins);
			break;
		case IC_CONSTANT:
			LoadConstant(iproc, ins);
			break;
		case IC_CALL:
//			CallFunction(iproc, ins);
			break;
		case IC_JSR:
//			CallAssembler(iproc, ins);
			break;
		case IC_PUSH_FRAME:
		{
		}	break;
		case IC_POP_FRAME:
		{
		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (sblock->mInstructions[i + 1].mCode == IC_BRANCH)
			{
				RelationalOperator(iproc, ins, proc->CompileBlock(iproc, sblock->mTrueJump), proc->CompileBlock(iproc, sblock->mFalseJump));
				return;
			}
			break;

		case IC_RETURN_VALUE:
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

			this->Close(proc->exitBlock, nullptr, ASMIT_JMP);
			return;
		}

		case IC_RETURN:
			this->Close(proc->exitBlock, nullptr, ASMIT_JMP);
			return;

		case IC_TYPECAST:
			break;

		case IC_BRANCH:
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mSIntConst[0] == 0)
					this->Close(proc->CompileBlock(iproc, sblock->mFalseJump), nullptr, ASMIT_JMP);
				else
					this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), nullptr, ASMIT_JMP);
			}
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));

				this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), proc->CompileBlock(iproc, sblock->mFalseJump), ASMIT_BNE);
			}
			return;

		}

		i++;
	}

	this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), nullptr, ASMIT_JMP);
}

void NativeCodeBasicBlock::Assemble(void)
{
	if (!mAssembled)
	{
		mAssembled = true;

		//PeepHoleOptimizer();

		for (int i = 0; i < mIns.Size(); i++)
			mIns[i].Assemble(this);

		if (this->mTrueJump)
			this->mTrueJump->Assemble();
		if (this->mFalseJump)
			this->mFalseJump->Assemble();
	}
}

void NativeCodeBasicBlock::Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch)
{
	this->mTrueJump = trueJump;
	this->mFalseJump = falseJump;
	this->mBranch = branch;
}


static int BranchByteSize(int from, int to)
{
	if (to - from >= -126 && to - from <= 129)
		return 2;
	else
		return 5;
}

static int JumpByteSize(int from, int to)
{
	return 3;
}

NativeCodeBasicBlock* NativeCodeBasicBlock::BypassEmptyBlocks(void)
{
	if (mBypassed)
		return this;
	else if (!mFalseJump && mCode.Size() == 0)
		return mTrueJump->BypassEmptyBlocks();
	else
	{
		mBypassed = true;

		if (mFalseJump)
			mFalseJump = mFalseJump->BypassEmptyBlocks();
		if (mTrueJump)
			mTrueJump = mTrueJump->BypassEmptyBlocks();

		return this;
	}
}

void NativeCodeBasicBlock::CopyCode(NativeCodeProcedure * proc, uint8* target)
{
	int i;
	int next;
	int pos, at;
	uint8 b;

	if (!mCopied)
	{
		mCopied = true;


		next = mOffset + mCode.Size();

		if (mFalseJump)
		{
			if (mFalseJump->mOffset <= mOffset)
			{
				if (mTrueJump->mOffset <= mOffset)
				{
					next += PutBranch(proc, mBranch, mTrueJump->mOffset - next);
					next += PutJump(proc, mFalseJump->mOffset - next);

				}
				else
				{
					next += PutBranch(proc, InvertBranchCondition(mBranch), mFalseJump->mOffset - next);
				}
			}
			else
			{
				next += PutBranch(proc, mBranch, mTrueJump->mOffset - next);
			}
		}
		else if (mTrueJump)
		{
			if (mTrueJump->mOffset != next)
			{
				next += PutJump(proc, mTrueJump->mOffset - next);
			}
		}

		assert(next - mOffset == mSize);

		for (i = 0; i < mCode.Size(); i++)
		{
			mCode.Lookup(i, target[i + mOffset]);
		}

		for (int i = 0; i < mRelocations.Size(); i++)
		{
			ByteCodeRelocation& rl(mRelocations[i]);
			rl.mAddr += mOffset;
			proc->mRelocations.Push(rl);
		}

		if (mTrueJump) mTrueJump->CopyCode(proc, target);
		if (mFalseJump) mFalseJump->CopyCode(proc, target);
	}
}

void NativeCodeBasicBlock::CalculateOffset(int& total)
{
	int next;

	if (mOffset > total)
	{
		mOffset = total;
		next = total + mCode.Size();

		if (mFalseJump)
		{
			if (mFalseJump->mOffset <= total)
			{
				// falseJump has been placed

				if (mTrueJump->mOffset <= total)
				{
					// trueJump and falseJump have been placed, not much to do

					next = next + BranchByteSize(next, mTrueJump->mOffset);
					total = next + JumpByteSize(next, mFalseJump->mOffset);
					mSize = total - mOffset;
				}
				else
				{
					// trueJump has not been placed, but falseJump has

					total = next + BranchByteSize(next, mFalseJump->mOffset);
					mSize = total - mOffset;
					mTrueJump->CalculateOffset(total);
				}
			}
			else if (mTrueJump->mOffset <= total)
			{
				// falseJump has not been placed, but trueJump has

				total = next + BranchByteSize(next, mTrueJump->mOffset);
				mSize = total - mOffset;
				mFalseJump->CalculateOffset(total);
			}
			else if (mKnownShortBranch)
			{
				// neither falseJump nor trueJump have been placed,
				// but we know from previous iteration that we can do
				// a short branch

				total = next + 2;
				mSize = total - mOffset;

				mFalseJump->CalculateOffset(total);
				if (mTrueJump->mOffset > total)
				{
					// trueJump was not placed in the process, so lets place it now
					mTrueJump->CalculateOffset(total);
				}
			}
			else
			{
				// neither falseJump nor trueJump have been placed
				// this may lead to some undo operation...
				// first assume a full size branch:

				total = next + 5;
				mSize = total - mOffset;

				mFalseJump->CalculateOffset(total);
				if (mTrueJump->mOffset > total)
				{
					// trueJump was not placed in the process, so lets place it now

					mTrueJump->CalculateOffset(total);
				}

				if (BranchByteSize(next, mTrueJump->mOffset) < 3)
				{
					// oh, we can replace by a short branch

					mKnownShortBranch = true;

					total = next + 2;
					mSize = total - mOffset;

					mFalseJump->CalculateOffset(total);
					if (mTrueJump->mOffset > total)
					{
						// trueJump was not placed in the process, so lets place it now

						mTrueJump->CalculateOffset(total);
					}
				}
			}
		}
		else if (mTrueJump)
		{
			if (mTrueJump->mOffset <= total)
			{
				// trueJump has been placed, so append the branch size

				total = next + JumpByteSize(next, mTrueJump->mOffset);
				mSize = total - mOffset;
			}
			else
			{
				// we have to place trueJump, so just put it right behind us

				total = next;
				mSize = total - mOffset;

				mTrueJump->CalculateOffset(total);
			}
		}
		else
		{
			// no exit from block

			total += mCode.Size();
			mSize = total - mOffset;
		}
	}
}

NativeCodeBasicBlock::NativeCodeBasicBlock(void)
	: mIns(NativeCodeInstruction(ASMIT_INV, ASMIM_IMPLIED)), mRelocations({ 0 })
{
	mTrueJump = mFalseJump = NULL;
	mOffset = 0x7fffffff;
	mCopied = false;
	mKnownShortBranch = false;
	mBypassed = false;
}

NativeCodeBasicBlock::~NativeCodeBasicBlock(void)
{

}

NativeCodeProcedure::NativeCodeProcedure(void)
	: mRelocations({ 0 })
{

}

NativeCodeProcedure::~NativeCodeProcedure(void)
{

}

void NativeCodeProcedure::Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc)
{
	tblocks = new NativeCodeBasicBlock * [proc->mBlocks.Size()];
	for (int i = 0; i < proc->mBlocks.Size(); i++)
		tblocks[i] = nullptr;

	mIndex = proc->mID;
	mNoFrame = true;

	entryBlock = new NativeCodeBasicBlock();
	entryBlock->mNoFrame = mNoFrame;
	
	tblocks[0] = entryBlock;

	exitBlock = new NativeCodeBasicBlock();
	exitBlock->mNoFrame = mNoFrame;

	exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_RTS, ASMIM_IMPLIED));

	entryBlock->Compile(proc, this, proc->mBlocks[0]);
	entryBlock->Assemble();

	int	total, base;

	NativeCodeBasicBlock* lentryBlock = entryBlock->BypassEmptyBlocks();

	total = 0;
	base = generator->mProgEnd;

	lentryBlock->CalculateOffset(total);	

	generator->AddAddress(proc->mID, true, base, total, proc->mIdent, true);

	lentryBlock->CopyCode(this, generator->mMemory + base);
	
	generator->mProgEnd += total;

	for (int i = 0; i < mRelocations.Size(); i++)
	{
		ByteCodeRelocation& rl(mRelocations[i]);
		rl.mAddr += base;
		generator->mRelocations.Push(rl);
	}
}

NativeCodeBasicBlock* NativeCodeProcedure::CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* sblock)
{
	if (tblocks[sblock->mIndex])
		return tblocks[sblock->mIndex];

	NativeCodeBasicBlock* block = new NativeCodeBasicBlock();
	block->mNoFrame = mNoFrame;

	tblocks[sblock->mIndex] = block;
	block->Compile(iproc, this, sblock);

	return block;
}
