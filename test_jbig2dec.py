#! /usr/bin/env python

# this is the testtest script for jbig2dec

import os, re
import unittest

class KnownFileHash(unittest.TestCase):
  # these are known test files in the form
  # (filename, sha-1(file), sha-1(decoded document)
  known_hashes = ( ('../ubc/042_1.jb2',
			"673e1ee5c55ab241b171e476ba1168a42733ddaa",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_2.jb2', 
                        "9aa2804e2d220952035c16fb3c907547884067c5",
                        "ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_3.jb2',
			"9663a5f35727f13e61a0a2f0a64207b1f79e7d67",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_4.jb2',
			"014df658c8b99b600c2ceac3f1d53c7cc2b4917c",
                        "ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_5.jb2',
			"264720a6ccbbf72aa6a2cfb6343f43b8e6f2da4b",
                        "ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_6.jb2',
			"96f7dc9df4a1b305f9ac082dd136f85ef5b108fe",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_7.jb2',
			"5526371ba9dc2b8743f20ae3e05a7e60b3dcba76",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_8.jb2',
			"4bf0c87dfaf40d67c36f2a083579eeda26d54641",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_9.jb2',
			"53e630e7fe2fe6e1d6164758e15fc93382e07f55",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_10.jb2',
			"5ca1364367e25cb8f642e9dc677a94d5cfed0c8b",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_11.jb2',
			"bc194caf022bc5345fc41259e05cea3c08245216",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_12.jb2',
			"f354df8eb4849bc707f088739e322d1fe3a14ef3",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_13.jb2',
			"7d428bd542f58591b254d9827f554b0552c950a7",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_14.jb2',
			"c40fe3a02acb6359baf9b40fc9c49bc0800be589",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_15.jb2',
			"a9e39fc1ecb178aec9f05039514d75ea3246246c",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_16.jb2',
			"4008bbca43670f3c90eaee26516293ba95baaf3d",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_17.jb2',
			"0ff95637b64c57d659a41c582da03e25321551fb",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_18.jb2',
			"87381d044f00c4329200e44decbe91bebfa31595",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_19.jb2',
			"387d95a140b456d4742622c788cf5b51cebbf438",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_20.jb2',
			"85c19e9ec42b8ddd6b860a1bebea1c67610e7a59",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_21.jb2',
			"ab535c7d7a61a7b9dc53d546e7419ca78ac7f447",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_22.jb2',
			"a9e2b365be63716dbde74b0661c3c6efd2a6844d",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_23.jb2',
			"8ffa40a05e93e10982b38a2233a8da58c1b5c343",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_24.jb2',
			"2553fe65111c58f6412de51d8cdc71651e778ccf",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/042_25.jb2',
			"52de4a3b86252d896a8d783ba71dd0699333dd69",
			"ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"),
                   ('../ubc/amb_1.jb2', # we don't have a correct hash for this yet
			"d6d6d1c981dc37a09108c1e3ed990aa5b345fa6a",
                        "ff32ffff0776ff66ff254129ff28ffffffff6bff"),
                   ('../ubc/amb_2.jb2', # we don't have a correct hash for this yet
			"9af6616a89eb03f8934de72626e301a716366c3c",
                        "ff32ffff0776ff66ff254129ff28ffffffff6bff"),
                   ('../str-p39',
			"1a303e33d3ea57eb7e19a676a1b2f28baa29b045",
                        "ff373f070f5f405b732c53ffffff087eff22ff5b") )

  def __init__(self, file, file_hash, decode_hash):
    self.file = file
    self.file_hash = file_hash
    self.decode_hash = decode_hash
    unittest.TestCase.__init__(self)

  def shortDescription(self):
    return "Checking '%s' for correct decoded document hash" % self.file

  def runTest(self):
    '''jbig2dec should return proper document hashes for known files'''
    # invoke jbig2dec on our file
    instance = os.popen('./jbig2dec -q -o /dev/null --hash ' + self.file)
    lines = instance.readlines()
    exit_code = instance.close()
    self.failIf(exit_code, 'jbig2dec should exit normally')
    # test here for correct hash
    hash_pattern = re.compile('[0-9a-f]{%d}' % len(decode_hash))
    for line in lines:
      m = hash_pattern.search(line.lower())
      if m:
        self.assertEqual(self.decode_hash, m.group(),
          'hash of known decoded document must be correct')
        return
    self.fail('document hash was not found in the output')

suite = unittest.TestSuite()
for filename, file_hash, decode_hash in KnownFileHash.known_hashes:
  # only add tests for files we can find
  if not os.access(filename, os.R_OK): continue
  # todo: verify our file matches its encoded document hash
  suite.addTest(KnownFileHash(filename, file_hash, decode_hash))

# run the defined tests if we're called as a script
if __name__ == "__main__":
    import sys
    verbosity = 2
    runner = unittest.TextTestRunner(verbosity=verbosity)
    result = runner.run(suite)
    sys.exit(not result.wasSuccessful())
