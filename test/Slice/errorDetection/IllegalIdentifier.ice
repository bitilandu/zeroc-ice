﻿// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************


//
// Check that 'œ' is properly rejected in idenifiers
//

module Test
{
   interface Œuvre
   {
      void cœur();
   };  
};