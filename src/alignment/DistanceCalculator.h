#ifndef MMSEQS_DISTANCECALCULATOR_H
#define MMSEQS_DISTANCECALCULATOR_H

#include <sstream>
#include <cstring>
#include <vector>

#include "simd.h"
#include "MathUtil.h"
#include "BaseMatrix.h"
#include "Parameters.h"

class DistanceCalculator {
public:
    template<typename T>
    static unsigned int computeSubstitutionDistance(const T *seq1,
                                                    const T *seq2,
                                                    const unsigned int length,
                                                    const char **subMat, bool globalAlignment = false) {
        int max = 0;
        int score = 0;
        if (globalAlignment)
        {
            for(unsigned int pos = 0; pos < length; pos++){
                max += subMat[static_cast<int>(seq1[pos])][static_cast<int>(seq2[pos])];
            }
        } else {
            for(unsigned int pos = 0; pos < length; pos++){
                int curr = subMat[static_cast<int>(seq1[pos])][static_cast<int>(seq2[pos])];
                score = curr  + score;
                score = (score < 0) ? 0 : score;
                max = (score > max)? score : max;
            }
        }
        max = (max<0) ? 0 : max;

        return max;
    }



    struct LocalAlignment{
        int startPos;
        int endPos;
        unsigned int score;
        unsigned int diagonalLen;
        unsigned int distToDiagonal;
        int diagonal;

        LocalAlignment(int startPos, int endPos, int score)
                : startPos(startPos), endPos(endPos), score(score), diagonalLen(0), distToDiagonal(0), diagonal(0)
        {}
        LocalAlignment() : startPos(-1), endPos(-1), score(0), diagonalLen(0), distToDiagonal(0), diagonal(0)  {}

    };

    template<typename T>
    static LocalAlignment computeUngappedAlignment(const T *querySeq, unsigned int querySeqLen,
                                                   const T *dbSeq, unsigned int dbSeqLen,
                                                   const unsigned short diagonal, const char **subMat, int alnMode){
        LocalAlignment max;
        for(unsigned int devisions = 1; devisions <= 1 + dbSeqLen / 32768; devisions++) {
            int realDiagonal = (-devisions * 65536  + diagonal);
            LocalAlignment tmp = ungappedAlignmentByDiagonal(querySeq, querySeqLen, dbSeq, dbSeqLen, realDiagonal, subMat, alnMode);
            if(tmp.score > max.score){
                max = tmp;
            }
        }
        for(unsigned int devisions = 0; devisions <= querySeqLen / 65536; devisions++) {
            int realDiagonal = (devisions * 65536 + diagonal);
            LocalAlignment tmp = ungappedAlignmentByDiagonal(querySeq, querySeqLen, dbSeq, dbSeqLen, realDiagonal, subMat, alnMode);
            if(tmp.score > max.score){
                max = tmp;
            }
        }
        return  max;
    }

    template<typename T>
    static LocalAlignment ungappedAlignmentByDiagonal(const T * querySeq, unsigned int querySeqLen,
                                                      const T * dbSeq,  unsigned int dbSeqLen,
                                                      int diagonal, const char **subMat, int alnMode) {
        unsigned int minDistToDiagonal = abs(diagonal);
        LocalAlignment res;
        res.distToDiagonal = minDistToDiagonal;
        res.diagonal = diagonal;
        if (diagonal >= 0 && minDistToDiagonal < querySeqLen) {
            unsigned int minSeqLen = std::min(dbSeqLen, querySeqLen - minDistToDiagonal);
            res.diagonalLen = minSeqLen;
            if (alnMode == Parameters::RESCORE_MODE_HAMMING) {
                res.score = DistanceCalculator::computeInverseHammingDistance(querySeq + minDistToDiagonal, dbSeq,
                                                                              minSeqLen);
            } else if (alnMode == Parameters::RESCORE_MODE_SUBSTITUTION) {
                res.score = DistanceCalculator::computeSubstitutionDistance(
                        querySeq + minDistToDiagonal, dbSeq, minSeqLen, subMat, false);
            } else if (alnMode == Parameters::RESCORE_MODE_ALIGNMENT) {
                LocalAlignment tmp = computeSubstitutionStartEndDistance(querySeq + minDistToDiagonal, dbSeq, minSeqLen, subMat);
                res.score = tmp.score;
                res.startPos = tmp.startPos;
                res.endPos = tmp.endPos;
            }
        } else if (diagonal < 0 && minDistToDiagonal < dbSeqLen) {
            unsigned int minSeqLen = std::min(dbSeqLen - minDistToDiagonal, querySeqLen);
            res.diagonalLen = minSeqLen;
            if (alnMode == Parameters::RESCORE_MODE_HAMMING) {
                res.score = DistanceCalculator::computeInverseHammingDistance(querySeq, dbSeq + minDistToDiagonal,
                                                                              minSeqLen);
            } else if (alnMode == Parameters::RESCORE_MODE_SUBSTITUTION) {
                res.score = DistanceCalculator::computeSubstitutionDistance(
                        querySeq, dbSeq + minDistToDiagonal, minSeqLen, subMat, false);
            } else if (alnMode == Parameters::RESCORE_MODE_ALIGNMENT) {
                LocalAlignment tmp = computeSubstitutionStartEndDistance(querySeq, dbSeq + minDistToDiagonal, minSeqLen, subMat);
                res.score = tmp.score;
                res.startPos = tmp.startPos;
                res.endPos = tmp.endPos;
            }
        }
        return res;
    }


    template<typename T>
    static LocalAlignment computeSubstitutionStartEndDistance(const T *seq1,
                                                              const T *seq2,
                                                              const unsigned int length,
                                                              const char **subMat) {
        int maxScore = 0;
        int maxEndPos = 0;
        int maxStartPos = 0;
        int minPos = -1;
//        int maxMinPos = 0;
        int score = 0;
        for(unsigned int pos = 0; pos < length; pos++){
            int curr = subMat[static_cast<int>(seq1[pos])][static_cast<int>(seq2[pos])];
            score = curr  + score;
            const bool isMinScore = (score <= 0);
            score =  (isMinScore) ? 0 : score;
            minPos = (isMinScore) ? pos : minPos;
            const bool isNewMaxScore = (score > maxScore);
            maxEndPos = (isNewMaxScore) ? pos : maxEndPos;
            maxStartPos = (isNewMaxScore) ? minPos + 1 : maxStartPos;
            maxScore = (isNewMaxScore)? score : maxScore;
        }
        return LocalAlignment(maxStartPos, maxEndPos, maxScore);
    }

    template<typename T>
    static unsigned int computeInverseHammingDistance(const T *seq1, const T *seq2, unsigned int length){
        unsigned int diff = 0;
        unsigned int simdBlock = length/(VECSIZE_INT*4);
        simd_int * simdSeq1 = (simd_int *) seq1;
        simd_int * simdSeq2 = (simd_int *) seq2;
        for (unsigned int pos = 0; pos < simdBlock; pos++ ) {
            simd_int seq1vec = simdi_loadu(simdSeq1+pos);
            simd_int seq2vec = simdi_loadu(simdSeq2+pos);
            // int _mm_movemask_epi8(__m128i a) creates 16-bit mask from most significant bits of
            // the 16 signed or unsigned 8-bit integers in a and zero-extends the upper bits.
            simd_int seqComparision = simdi8_eq(seq1vec, seq2vec);
            int res = simdi8_movemask(seqComparision);
            diff += MathUtil::popCount(res);  // subtract positions that should not contribute to coverage
        }
        // compute missing rest
        for (unsigned int pos = simdBlock*(VECSIZE_INT*4); pos < length; pos++ ) {
            diff += (seq1[pos] == seq2[pos]);
        }
        return diff;
    }


    /*
     * Adapted from levenshtein.js (https://gist.github.com/andrei-m/982927)
     * Changed to hold only one row of the dynamic programing matrix
     * Copyright (c) 2011 Andrei Mackenzie
     * Martin Steinegger: Changed to local alignment version
     * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
     * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
     * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
    static int localLevenshteinDistance(const std::string &s1, const std::string &s2) {
        int m = s1.length();
        int n = s2.length();

        if (m == 0)
            return n;
        if (n == 0)
            return m;

        // we don't need the full matrix just for the distance
        // we only need space for one row + the last value;
        int *currRow = new int[m + 1];
        for (int i = 0; i <= m; ++i) {
            currRow[i] = 0;
        }
        int maxVal=0;
        int val;
        // fill in the rest of the matrix
        for (int i = 1; i <= n; i++) {
            int prev = 0;
            for (int j = 1; j <= m; j++) {
                int subScore = (s2[i - 1] == s1[j - 1]) ? currRow[j - 1] + 1 : currRow[j - 1] - 1;
                val = std::max(0,
                               std::max(subScore, // substitution
                                        std::max(prev - 1,       // insertion
                                                 currRow[j] - 1)));   // deletion


                if(val > maxVal){
                    maxVal = val;
                }
                currRow[j - 1] = prev;
                prev = val;
            }
//            std::swap(currRow, topRow);
        }

        // the last element of the matrix is the distance
        delete[] currRow;

        return maxVal;
    }

    static double keywordDistance(const std::string &s1, const std::string &s2) {
        std::stringstream seq1(s1);
        std::string segment;
        std::vector<std::string> kwlist1;
        while (std::getline(seq1, segment, ';')) {
            if (strcmp(segment.c_str(), "\n") != 0)
                kwlist1.push_back(segment);
        }
        std::vector<std::string> kwlist2;
        std::stringstream seq2(s2);
        while (std::getline(seq2, segment, ';')) {
            if (strcmp(segment.c_str(), "\n") != 0)
                kwlist2.push_back(segment);
        }
        double score = 0.0;
        for (std::string s1 : kwlist1) {
            for (std::string s2 : kwlist2) {
                if (strcmp(s1.c_str(), s2.c_str()) == 0) {
                    score++;
                    break;
                }
            }
        }
        score = ((score / std::max(kwlist1.size(), kwlist2.size())) +
                 (score / std::min(kwlist1.size(), kwlist2.size()))) / 2;

        return score;
    }

    void prepareGlobalAliParam(const BaseMatrix &subMat) {
        globalAliMu = 0;
        globalAliSigma = 0;

        for (int i = 0; i < (subMat.alphabetSize - 1); i++) {
            for (int j = 0; j < (subMat.alphabetSize - 1); j++) {
                globalAliMu += subMat.pBack[i] * subMat.pBack[j] * subMat.subMatrix[i][j];
            }
        }

        for (int i = 0; i < (subMat.alphabetSize - 1); i++) {
            for (int j = 0; j < (subMat.alphabetSize - 1); j++) {
                double distToMean = (subMat.subMatrix[i][j] - globalAliMu);
                globalAliSigma += subMat.pBack[i] * subMat.pBack[j] * distToMean * distToMean;
            }
        }
        globalAliSigma = sqrt(globalAliSigma);

    }

    double getPvalGlobalAli(float score, size_t len) {
        return 0.5 - 0.5 * erf((score / len - globalAliMu) / (sqrt(2.0 / sqrt((float) len)) * globalAliSigma));
    }

private:
    float globalAliMu, globalAliSigma;

};

#endif //MMSEQS_DISTANCECALCULATOR_H
