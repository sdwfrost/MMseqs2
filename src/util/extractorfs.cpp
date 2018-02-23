#include <unistd.h>
#include <climits>
#include <algorithm>

#include "Debug.h"
#include "Parameters.h"
#include "DBReader.h"
#include "DBWriter.h"
#include "Util.h"
#include "itoa.h"

#include "Orf.h"

#ifdef OPENMP
#include <omp.h>

#endif

unsigned int getFrames(std::string frames) {
    unsigned int result = 0;

    std::vector<std::string> frame = Util::split(frames, ",");

    if(std::find(frame.begin(), frame.end(), "1") != frame.end()) {
        result |= Orf::FRAME_1;
    }

    if(std::find(frame.begin(), frame.end(), "2") != frame.end()) {
        result |= Orf::FRAME_2;
    }

    if(std::find(frame.begin(), frame.end(), "3") != frame.end()) {
        result |= Orf::FRAME_3;
    }

    return result;
}

int extractorfs(int argc, const char **argv, const Command& command) {
    Parameters& par = Parameters::getInstance();
    par.parseParameters(argc, argv, command, 2);

#ifdef OPENMP
    omp_set_num_threads(par.threads);
#endif

    DBReader<unsigned int> reader(par.db1.c_str(), par.db1Index.c_str());
    reader.open(DBReader<unsigned int>::NOSORT);

    std::string headerIn(par.db1);
    headerIn.append("_h");

    std::string headerInIndex(par.db1);
    headerInIndex.append("_h.index");

    DBReader<unsigned int> headerReader(headerIn.c_str(), headerInIndex.c_str());
    headerReader.open(DBReader<unsigned int>::NOSORT);

    DBWriter sequenceWriter(par.db2.c_str(), par.db2Index.c_str(), par.threads);
    sequenceWriter.open();

    std::string headerOut(par.db2);
    headerOut.append("_h");

    std::string headerIndexOut(par.db2);
    headerIndexOut.append("_h.index");

    DBWriter headerWriter(headerOut.c_str(), headerIndexOut.c_str(), par.threads);
    headerWriter.open();


    std::string dbSetToOrf = par.db2 + "_set_lookup";
    std::string dbIndexSetToOrf = par.db2 + "_set_lookup.index";
    DBWriter writerSetToOrf(dbSetToOrf.c_str(), dbIndexSetToOrf.c_str(), par.threads);
    writerSetToOrf.open();

    std::string dbOrfToSet = par.db2 + "_orf_lookup";
    std::string dbIndexOrfToSet = par.db2 + "_orf_lookup.index";
    DBWriter writerOrfToSet(dbOrfToSet.c_str(), dbIndexOrfToSet.c_str(), par.threads);
    writerOrfToSet.open();

    unsigned int forwardFrames = getFrames(par.forwardFrames);
    unsigned int reverseFrames = getFrames(par.reverseFrames);

    unsigned int extendMode = 0;
    if(par.orfLongest)
        extendMode |= Orf::EXTEND_START;

    if(par.orfExtendMin)
        extendMode |= Orf::EXTEND_END;

    unsigned int total = 0;
    #pragma omp parallel for schedule(static) shared(total) 

    for (unsigned int i = 0; i < reader.getSize(); ++i){
        unsigned int id;
        Orf orf;
        Debug::printProgress(i);
        int thread_idx = 0;
        #ifdef OPENMP
        thread_idx = omp_get_thread_num();
        #endif

        std::string orfsBuffer;
        orfsBuffer.reserve(10000);

        unsigned int key = reader.getDbKey(i);
        std::string data(reader.getData(i));
        // remove newline in sequence
        data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());

        if(!orf.setSequence(data.c_str(), data.length())) {
            Debug(Debug::WARNING) << "Invalid sequence with index " << i << "!\n";
            continue;
        }

        std::string header(headerReader.getData(i));
        // remove newline in header
        header.erase(std::remove(header.begin(), header.end(), '\n'), header.end());

        std::vector<Orf::SequenceLocation> res;
        orf.findAll(res, par.orfMinLength, par.orfMaxLength, par.orfMaxGaps, forwardFrames, reverseFrames, extendMode);
        for (std::vector<Orf::SequenceLocation>::const_iterator it = res.begin(); it != res.end(); ++it) {
            Orf::SequenceLocation loc = *it;

            #pragma omp critical
            {
                total++;
                id = total + par.identifierOffset;
            }

            if (par.orfSkipIncomplete && (loc.hasIncompleteStart || loc.hasIncompleteEnd))
                continue;

            char buffer[LINE_MAX];
            snprintf(buffer, LINE_MAX, "%s [Orf: %u, %zu, %zu, %d, %d, %d]\n", header.c_str(), key, loc.from, loc.to, loc.strand, loc.hasIncompleteStart, loc.hasIncompleteEnd);

            headerWriter.writeData(buffer, strlen(buffer), id, thread_idx);

            std::string sequence = orf.view(loc);
            sequence.append("\n");
            sequenceWriter.writeData(sequence.c_str(), sequence.length(), id, thread_idx);

            Itoa::u32toa_sse2(static_cast<uint32_t>(key), buffer);
            std::string setBuffer(buffer);
            setBuffer.append("\n");
            writerOrfToSet.writeData(setBuffer.c_str(), setBuffer.length(), id, thread_idx);

            Itoa::u32toa_sse2(static_cast<uint32_t>(id), buffer);
            orfsBuffer.append(buffer);
            orfsBuffer.append("\n");
        }
        writerSetToOrf.writeData(orfsBuffer.c_str(), orfsBuffer.length(), key, thread_idx);
    }

    writerSetToOrf.close();
    writerOrfToSet.close();

    headerWriter.close();
    sequenceWriter.close(DBReader<unsigned int>::DBTYPE_NUC);
    headerReader.close();
    reader.close();
    
    return EXIT_SUCCESS;
}
