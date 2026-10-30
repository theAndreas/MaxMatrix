/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       MaxMatrixWired.h
 *      \brief      The matrix as it is wired
 *
 *      \details    The matrix buffer and what sends it: columns as the MAX7219 drive them, without any orientation.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_WIRED_H_
#define _MAXMATRIX_WIRED_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "Max7219/MaxMatrixChain.h"


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixWired
 *****************************************************************************************************************************************************/
class MaxMatrixWired
{
  private:
    MaxMatrixChain Chain;
    byte MatrixBuffer[MAXMATRIX_BUFFER_SIZE];

  public:
    MaxMatrixWired(byte, byte, byte);

    // get methods
    MaxMatrixChain& getChain() { return Chain; }
    /* the buffer alone, unchecked: for MaxMatrixPanel, whose coordinates are checked and mapped already */
    byte getColumn(byte Column) { return MatrixBuffer[Column]; }

    // set methods
    /* the buffer alone, unchecked; sendColumn() or reload() sends it */
    void storeBit(byte Column, byte Bit, bool Value) { bitWrite(MatrixBuffer[Column], Bit, Value); }
    stdReturnType setColumn(byte, byte);
    stdReturnType setColumnOnAllModules(byte, byte);

    // methods
    void sendColumn(byte Column) { setColumn(Column, MatrixBuffer[Column]); }
    void clear();
    void reload();
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
