/*
 * Author   : Saurabh Shrivastava
 * Email    : saurabh.shrivastava54@gmail.com
 * Link     : https://github.com/saurabhshri
 */

#include "recognize_using_pocketsphinx.h"

PocketsphinxAligner::PocketsphinxAligner(Params *parameters)
{
    _parameters = parameters;

    //creating local copies
    _audioFileName = _parameters->audioFileName;
    _subtitleFileName = _parameters->subtitleFileName;
    _outputFileName = _parameters->outputFileName;

    _modelPath = _parameters->modelPath;
    _dictPath = _parameters->dictPath;
    _lmPath = _parameters->lmPath;
    _fsgPath = _parameters ->fsgPath;
    _logPath = _parameters->logPath;
    _phoneticlmPath = _parameters->phoneticlmPath;
    _phonemeLogPath = _parameters->phonemeLogPath;

    _audioWindow = _parameters->audioWindow;
    _sampleWindow = _parameters->sampleWindow;
    _searchWindow = _parameters->searchWindow;

    _alignedData = new AlignedData();

    processFiles();
}

bool PocketsphinxAligner::processFiles()
{
    LOG("Initialising Aligner using PocketSphinx");
    LOG("Audio Filename: %s Subtitle filename: %s", _audioFileName.c_str(), _subtitleFileName.c_str());

    std::cout << "Reading and processing subtitles...\n";
    _subParserFactory = new SubtitleParserFactory(_subtitleFileName);
    _parser = _subParserFactory->getParser();
    _subtitles = _parser->getSubtitles();

    std::cout << "Reading and decoding audio samples...\n";
    if(_parameters->readStream)
        _file = new WaveFileData(readStreamDirectly, _parameters->audioIsRaw);
    else
        _file = new WaveFileData(_audioFileName, _parameters->audioIsRaw);

    _file->read();
    _samples = _file->getSamples();

    return true;

}

bool PocketsphinxAligner::generateGrammar(grammarName name)
{
    LOG("Generating Grammar based on subtitles, Grammar Name: %d ", name);

    std::cout << "Generating language model and grammar files...\n";

    if(_parameters->grammarType == complete_grammar || _parameters->grammarType == dict)
    {
        std::cout << "Note: You have chosen to generate a dictionary. Based on your TensorFlow configuration,\n";
        std::cout << "this may take some time, please be patient. For alternatives, see docs.\n";
    }

    return generate(_subtitles, name);
}

bool PocketsphinxAligner::initDecoder(std::string modelPath, std::string lmPath, std::string dictPath, std::string fsgPath, std::string logPath)
{
    LOG("Initialising PocketSphinx decoder");

    std::cout << "Initialising PocketSphinx decoder..\n";
    _modelPath = modelPath;
    _lmPath = lmPath;
    _dictPath = dictPath;
    _fsgPath = fsgPath;
    _logPath = logPath;

    LOG("Configuration: \n\tmodelPath = %s \n\tlmPath = %s \n\tdictPath = %s \n\tfsgPath = %s \n\tlogPath = %s ",
        _modelPath.c_str(), _lmPath.c_str(), _dictPath.c_str(), _fsgPath.c_str(), _logPath.c_str());

    if(_parameters->useBatchMode)
    {
        if(_parameters->useExperimentalParams)
        {
            _configWord = cmd_ln_init(NULL,
                                      ps_args(), TRUE,
                                      "-hmm", modelPath.c_str(),
                                      "-lm", lmPath.c_str(),
                                      "-dict", dictPath.c_str(),
                                      "-logfn", logPath.c_str(),
                                      "-cmn", "batch",
                                      "-lw", "1.0",
                                      "-beam", "1e-80",
                                      "-wbeam", "1e-60",
                                      "-pbeam", "1e-80",
                                      NULL);
        }

        else
        {
            _configWord = cmd_ln_init(NULL,
                                      ps_args(), TRUE,
                                      "-hmm", modelPath.c_str(),
                                      "-lm", lmPath.c_str(),
                                      "-dict", dictPath.c_str(),
                                      "-logfn", logPath.c_str(),
                                      "-cmn", "batch",
                                      NULL);
        }

    }

    else if(_parameters->useExperimentalParams)
    {
        _configWord = cmd_ln_init(NULL,
                                  ps_args(), TRUE,
                                  "-hmm", modelPath.c_str(),
                                  "-lm", lmPath.c_str(),
                                  "-dict", dictPath.c_str(),
                                  "-logfn", logPath.c_str(),
                                  "-lw", "1.0",
                                  "-beam", "1e-80",
                                  "-wbeam", "1e-60",
                                  "-pbeam", "1e-80",
                                  NULL);

    }

    else
    {
        _configWord = cmd_ln_init(NULL,
                                  ps_args(), TRUE,
                                  "-hmm", modelPath.c_str(),
                                  "-lm", lmPath.c_str(),
                                  "-dict", dictPath.c_str(),
                                  "-logfn", logPath.c_str(),
                                  NULL);
    }


    if (_configWord == NULL)
    {
        FATAL(EXIT_FAILURE, "Failed to create config object, see log for details" );
    }

    _psWordDecoder = ps_init(_configWord);

    if (_psWordDecoder == NULL)
    {
        FATAL(EXIT_FAILURE, "Failed to create recognizer, see log for details" );
    }

    if(_parameters->searchPhonemes)
    {
        initPhonemeDecoder(_parameters->phoneticlmPath, _parameters->phonemeLogPath);
    }

    return true;
}


bool PocketsphinxAligner::initPhonemeDecoder(std::string phoneticlmPath, std::string phonemeLogPath)
{
    LOG("Initialising PocketSphinx phoneme decoder");

    std::cout << "Initialising PocketSphinx phoneme decoder..\n";

    _phoneticlmPath = phoneticlmPath;
    _phonemeLogPath = phonemeLogPath;

    LOG("Configuration : \n\tphoneticlmPath = %s \n\tphonemeLogPath = %s", _phoneticlmPath.c_str(), _phonemeLogPath.c_str());

    _configPhoneme = cmd_ln_init(NULL,
                             ps_args(), TRUE,
                             "-hmm", _modelPath.c_str(),
                             "-lm", _lmPath.c_str(),
                             "-logfn", _phonemeLogPath.c_str(),
                             "-allphone", _phoneticlmPath.c_str(),
                             "-beam", "1e-20",
                             "-pbeam", "1e-10",
                             "-allphone_ci", "no",
                             "-backtrace", "yes",
                             "-lw", "2.0",
                             NULL);

    if (_configPhoneme == NULL)
    {
        FATAL(EXIT_FAILURE, "Failed to create config object, see log for details" );
    }

    _psPhonemeDecoder = ps_init(_configPhoneme);

    if (_psPhonemeDecoder == NULL)
    {
        FATAL(EXIT_FAILURE, "Failed to create phoneme recognizer, see log for details" );
    }

    return true;

}

int levenshtein_distance(const std::string &firstWord, const std::string &secondWord)
{
    const unsigned long int length1 = firstWord.size();
    const unsigned long int length2 = secondWord.size();

    std::vector<int> currentColumn(length2 + 1);
    std::vector<int> previousColumn(length2 + 1);

    for (int index2 = 0; index2 < length2 + 1; ++index2)
    {
        previousColumn[index2] = index2;
    }

    for (int index1 = 0; index1 < length1; ++index1)
    {
        currentColumn[0] = index1 + 1;

        for (int index2 = 0; index2 < length2; ++index2)
        {
            const int compare = firstWord[index1] == secondWord[index2] ? 0 : 1;

            currentColumn[index2 + 1] = std::min(std::min(currentColumn[index2] + 1, previousColumn[index2 + 1] + 1), previousColumn[index2] + compare);
        }

        currentColumn.swap(previousColumn);
    }

    return previousColumn[length2];
}

bool PocketsphinxAligner::findAndSetPhonemeTimes(cmd_ln_t *config, ps_decoder_t *ps, SubtitleItem *sub)
{
    ps_start_stream(ps);
    int frame_rate = cmd_ln_int32_r(config, "-frate");
    ps_seg_t *iter = ps_seg_iter(ps);

    recognisedBlock currentBlock; //storing recognised words and their timing information

    while (iter != NULL)
    {
        int32 sf, ef, pprob;
        float conf;

        ps_seg_frames(iter, &sf, &ef);
        pprob = ps_seg_prob(iter, NULL, NULL, NULL);
        conf = logmath_exp(ps_get_logmath(ps), pprob);

        std::string recognisedPhoneme(ps_seg_word(iter));
        //the time when utterance was marked, the times are w.r.t. to this
        long int startTime = sub->getStartTime();
        long int endTime = startTime;

        if (recognisedPhoneme == "SIL" || recognisedPhoneme == "BREATH" || recognisedPhoneme == "SMACK" || recognisedPhoneme == "NOISE" || recognisedPhoneme[0] == '+' || recognisedPhoneme[0] == '[')
            goto skipSearchingThisPhoneme;

        /*
         * Finding start time and end time of each word.
         *
         * 1 sec = 1000 ms, thus time in second = 1000 / frame rate.
         *
         */

        startTime += sf * 1000 / frame_rate;
        endTime += ef * 1000 / frame_rate;

        sub->addPhoneme(recognisedPhoneme,startTime,endTime);
     skipSearchingThisPhoneme:
        iter = ps_seg_next(iter);
    }

    return true;
}

recognisedBlock PocketsphinxAligner::findAndSetWordTimes(cmd_ln_t *config, ps_decoder_t *ps, SubtitleItem *sub)
{
    ps_start_stream(ps);
    int frame_rate = cmd_ln_int32_r(config, "-frate");
    ps_seg_t *iter = ps_seg_iter(ps);

    std::vector<std::string> words = sub->getIndividualWords();

    //converting locally stored words into lowercase - as the recognised words are in lowercase

    for (std::string &eachWord : words)
    {
        std::transform(eachWord.begin(), eachWord.end(), eachWord.begin(), ::tolower);
    }

    int lastWordFoundAtIndex = -1;

    recognisedBlock currentBlock; //storing recognised words and their timing information

    while (iter != NULL)
    {
        int32 sf, ef, pprob;
        float conf;

        ps_seg_frames(iter, &sf, &ef);
        pprob = ps_seg_prob(iter, NULL, NULL, NULL);
        conf = logmath_exp(ps_get_logmath(ps), pprob);

        std::string recognisedWord(ps_seg_word(iter));

        //the time when utterance was marked, the times are w.r.t. to this
        long int startTime = sub->getStartTime();
        long int endTime = startTime;

        /*
         * Finding start time and end time of each word.
         *
         * 1 sec = 1000 ms, thus time in second = 1000 / frame rate.
         *
         */

        startTime += sf * 1000 / frame_rate;
        endTime += ef * 1000 / frame_rate;

        if(startTime > endTime)
            FATAL(EXIT_INVALID_PARAMETERS, "Error setting start and end time.");

        //storing recognised words and their timing information
        currentBlock.recognisedString.push_back(recognisedWord);
        currentBlock.recognisedWordStartTimes.push_back(startTime);
        currentBlock.recognisedWordEndTimes.push_back(endTime);


        /*
         * Suppose this is the case :
         *
         * Actual      : [Why] would you use a tomato just why
         * Recognised  : would you use a tomato just [why]
         *
         * So, if we search whole recognised sentence for actual words one by one, then Why[1] of Actual will get associated
         * with why[7] of recognised. Thus limiting the number of words it can look ahead.
         *
         */

        int searchWindowSize = _searchWindow;

        /*
            Recognised  : so have you can you've brought seven
                               |
                        ---------------
                        |               |
            Actual      : I think you've brought with you

            Recognised  : so have you can you've brought seven
                                    |
                            -------------------
                            |                  |
            Actual      : I think you've brought with you

         */

        //Do not try to search silence and words like [BREATH] et cetera..
        if (recognisedWord == "<s>" || recognisedWord == "</s>" || recognisedWord[0] == '[' || recognisedWord == "<sil>")
            goto skipSearchingThisWord;

        for (int wordIndex = lastWordFoundAtIndex + 1; wordIndex < words.size(); wordIndex++)
        {
            if (wordIndex > (int) currentBlock.recognisedString.size() + searchWindowSize)
                break;

            int distance = levenshtein_distance(words[wordIndex], recognisedWord);
            int largerLength = words[wordIndex].size() > recognisedWord.size() ? words[wordIndex].size() : recognisedWord.size();

            if (distance < largerLength * 0.25)   //at least 75% must match
            {

                lastWordFoundAtIndex = wordIndex;
                sub->setWordRecognisedStatusByIndex(true, wordIndex);
                sub->setWordTimesByIndex(startTime, endTime, wordIndex);

                if(_parameters->displayRecognised)
                {
                    std::cout << "Possible Match : " << words[wordIndex];
                    std::cout << "\t\tStart : \t\t" << sub->getWordStartTimeByIndex(wordIndex);
                    std::cout << "\tEnd : \t\t" << sub->getWordEndTimeByIndex(wordIndex);
                    std::cout << "\tDuration : \t\t" << sub->getWordEndTimeByIndex(wordIndex) - sub->getWordStartTimeByIndex(wordIndex);
                    std::cout << "\n";
                }

                break;
            }

        }

skipSearchingThisWord:
        iter = ps_seg_next(iter);
    }

    return currentBlock;
}

bool PocketsphinxAligner::printWordTimes(cmd_ln_t *config, ps_decoder_t *ps)
{
    ps_start_stream(ps);
    int frame_rate = cmd_ln_int32_r(config, "-frate");
    ps_seg_t *iter = ps_seg_iter(ps);
    while (iter != NULL)
    {
        int32 sf, ef, pprob;
        float conf;

        ps_seg_frames(iter, &sf, &ef);
        pprob = ps_seg_prob(iter, NULL, NULL, NULL);
        conf = logmath_exp(ps_get_logmath(ps), pprob);
        printf(">>> %s \t %.3f \t %.3f\n", ps_seg_word(iter), ((float) sf / frame_rate),
               ((float) ef / frame_rate));
        iter = ps_seg_next(iter);
    }

    return true;
}

bool PocketsphinxAligner::recognise()
{
    int subCount = 1;
    initFile(_outputFileName, _parameters->outputFormat);

    long int recognitionWindow = 0;

    if(_audioWindow)
    {
        recognitionWindow = _audioWindow * 16;
    }

    else if(_sampleWindow)
    {
        recognitionWindow = _sampleWindow;
    }

    std::cout << "Recognising and aligning..\n";

    for (SubtitleItem *sub : _subtitles)
    {
        if (sub->getDialogue().empty())
            continue;


        //first assigning approx timestamps
        CurrentSub *currSub = new CurrentSub(sub);
        currSub->run();

        //let's correct the timestamps :)

        long int dialogueStartsAt = sub->getStartTime();
        long int dialogueLastsFor = (sub->getEndTime() - dialogueStartsAt);

        long int samplesAlreadyRead = dialogueStartsAt * 16;
        long int samplesToBeRead = dialogueLastsFor * 16;

        if((samplesAlreadyRead - recognitionWindow) >= 0)
            samplesAlreadyRead -= recognitionWindow;
        else
            samplesAlreadyRead = 0;

        if((samplesToBeRead + (2 * recognitionWindow)) < _samples.size())
            samplesToBeRead += (2 * recognitionWindow);

        else
            samplesToBeRead = _samples.size() - 1;

        /*
         * 00:00:19,320 --> 00:00:21,056
         * Why are you boring?
         *
         * dialogueStartsAt : 19320 ms
         * dialogueEndsAt   : 21056 ms
         * dialogueLastsFor : 1736 ms
         *
         * SamplesAlreadyRead = 19320 ms * 16 samples/ms = 309120 samples
         * SampleToBeRead     = 1736  ms * 16 samples/ms = 27776 samples
         *
         */

        const int16_t *sample = _samples.data();

        _rvWord = ps_start_utt(_psWordDecoder);
        _rvWord = ps_process_raw(_psWordDecoder, sample + samplesAlreadyRead, samplesToBeRead, FALSE, FALSE);
        _rvWord = ps_end_utt(_psWordDecoder);

        _hypWord = ps_get_hyp(_psWordDecoder, &_scoreWord);

        if (_hypWord == NULL)
        {
            _hypWord = "NULL";

            if(_parameters->displayRecognised)
            {
                std::cout << "\n\n-----------------------------------------\n\n";
                std::cout << "Recognised: " << _hypWord << "\n";
            }

            continue;

        }

        if(_parameters->displayRecognised)
        {
            std::cout << "\n\n-----------------------------------------\n\n";
            std::cout << "Start time of dialogue : " << dialogueStartsAt << "\n";
            std::cout << "End time of dialogue   : " << sub->getEndTime() << "\n\n";
            std::cout << "Recognised  : " << _hypWord << "\n";
            std::cout << "Actual      : " << sub->getDialogue() << "\n\n";
        }

        //finding and aligning words from subtitle
        recognisedBlock currBlock = findAndSetWordTimes(_configWord, _psWordDecoder, sub);

        //trying to align non recognised words
        currSub->alignNonRecognised(currBlock);

        if(_parameters->searchPhonemes)
            recognisePhonemes(sample + samplesAlreadyRead, samplesToBeRead, sub);

        switch (_parameters->outputFormat)  //decide on basis of set output format
        {
            case srt:       subCount = printSRTContinuous(_outputFileName, subCount, sub, _parameters->printOption);
                break;

            case xml:       printXMLContinuous(_outputFileName, sub);
                break;

            case json:      printJSONContinuous(_outputFileName, sub);
                break;

            case karaoke:   subCount = printKaraokeContinuous(_outputFileName, subCount, sub, _parameters->printOption);
                break;

            default:        std::cout<<"An error occurred while choosing output format!";
                exit(2);
        }

        delete currSub;
    }

    printFileEnd(_outputFileName, _parameters->outputFormat);

    std::cout << "Finished recognition and alignment..\n";

    return true;
}

bool PocketsphinxAligner::align()
{
    if(_parameters->grammarType != no_grammar)
        generateGrammar(_parameters->grammarType);

    initDecoder(_parameters->modelPath, _parameters->lmPath, _parameters->dictPath, _parameters->fsgPath, _parameters->logPath);

    if(_parameters->transcribe)
    {
        transcribe();
    }

    else
    {
        if(_parameters->useFSG)
            alignWithFSG();
        else
            recognise();
    }

    return true;

}

bool PocketsphinxAligner::recognisePhonemes(const int16_t *sample, int readLimit, SubtitleItem *sub)
{
    _rvPhoneme = ps_start_utt(_psPhonemeDecoder);
    _rvPhoneme = ps_process_raw(_psPhonemeDecoder, sample, readLimit, FALSE, FALSE);
    _rvPhoneme = ps_end_utt(_psPhonemeDecoder);

    _hypPhoneme = ps_get_hyp(_psPhonemeDecoder, &_scorePhoneme);

    if (_hypPhoneme == NULL)
    {
        _hypPhoneme = "NULL";

        if(_parameters->displayRecognised)
            std::cout << "Phonemes: " << _hypPhoneme << "\n";
    }

    else
    {
        if(_parameters->displayRecognised)
            std::cout<<"Phonemes: "<<_hypPhoneme<<"\n";

        findAndSetPhonemeTimes(_configPhoneme,_psPhonemeDecoder, sub);
    }

    return true;
}

int PocketsphinxAligner::findTranscribedWordTimings(cmd_ln_t *config, ps_decoder_t *ps, int index)
{
    int frame_rate = cmd_ln_int32_r(config, "-frate");
    ps_seg_t *iter = ps_seg_iter(ps);
    int printedTillIndex = index;

    while (iter != NULL)
    {
        index++;
        int32 sf, ef, pprob;
        float conf;

        ps_seg_frames(iter, &sf, &ef);
        pprob = ps_seg_prob(iter, NULL, NULL, NULL);
        conf = logmath_exp(ps_get_logmath(ps), pprob);

        std::string recognisedWord(ps_seg_word(iter));
        long startTime = sf * 1000 / frame_rate, endTime = ef * 1000 / frame_rate;

        _alignedData->addNewWord(recognisedWord, startTime, endTime, conf);

        iter = ps_seg_next(iter);
    }

    if(_parameters->outputFormat == xml)
        printTranscriptionAsXMLContinuous(_outputFileName, _alignedData, printedTillIndex);

    else if(_parameters->outputFormat == json)
        printTranscriptionAsJSONContinuous(_outputFileName, _alignedData, printedTillIndex);

    else if(_parameters->outputFormat == srt)
        printTranscriptionAsSRTContinuous(_outputFileName, _alignedData, printedTillIndex);

    return index;
}

bool PocketsphinxAligner::transcribe()
{
    std::cout << "Transcribing...\n";

    //pointer to samples
    const int16_t *sample = _samples.data();

    //creating partition of 2048 bytes
    int numberOfPartitions = _samples.size() / 2048, remainingSamples = _samples.size() % 2048;

    //index of the word : used for sub and output handling
    int index=0;

    bool utt_started, in_speech;

    _rvWord = ps_start_utt(_psWordDecoder);
    utt_started = FALSE;

    printTranscriptionHeader(_outputFileName, _parameters->outputFormat);

    for (int i = 0; i <= numberOfPartitions; i++)
    {
        if (i == numberOfPartitions)
            ps_process_raw(_psWordDecoder, sample, remainingSamples, FALSE, FALSE);

        else
            ps_process_raw(_psWordDecoder, sample, 2048, FALSE, FALSE);

        in_speech = ps_get_in_speech(_psWordDecoder);

        if (in_speech && !utt_started)
        {
            utt_started = TRUE;
        }

        if (!in_speech && utt_started)
        {
            ps_end_utt(_psWordDecoder);
            _hypWord = ps_get_hyp(_psWordDecoder, NULL);

            if (_hypWord != NULL)
            {
                if(_parameters->displayRecognised)
                    std::cout << "Recognised: " << _hypWord << "\n";
                index = findTranscribedWordTimings(_configWord, _psWordDecoder, index);
            }

            ps_start_utt(_psWordDecoder);
            utt_started = FALSE;
        }

        sample += 2048;

    }

    _rvWord = ps_end_utt(_psWordDecoder);

    if (utt_started)
    {
        _hypWord = ps_get_hyp(_psWordDecoder, NULL);
        if (_hypWord != NULL)
        {
            if(_parameters->displayRecognised)
                std::cout << "Recognised: " << _hypWord << "\n";
            index = findTranscribedWordTimings(_configWord, _psWordDecoder, index);
        }
    }

    printTranscriptionFooter(_outputFileName, _parameters->outputFormat);

    std::cout << "Finished transcription.\n";

    return true;
}

bool PocketsphinxAligner::reInitDecoder(cmd_ln_t *config, ps_decoder_t *ps)
{
    ps_reinit(_psWordDecoder, _configWord);
    return true;
}

bool PocketsphinxAligner::alignWithFSG()
{
    int subCount = 1;
    initFile(_outputFileName, _parameters->outputFormat);

    long int recognitionWindow = 0;

    if(_audioWindow)
    {
        recognitionWindow = _audioWindow * 16;
    }

    else if(_sampleWindow)
    {
        recognitionWindow = _sampleWindow;
    }

    for (SubtitleItem *sub : _subtitles)
    {
        if (sub->getDialogue().empty())
            continue;


        //first assigning approx timestamps
        CurrentSub *currSub = new CurrentSub(sub);
        currSub->run();

        long int dialogueStartsAt = sub->getStartTime();
        std::string fsgname(_fsgPath + std::to_string(dialogueStartsAt));
        fsgname += ".fsg";

        cmd_ln_t *subConfig;
        subConfig = cmd_ln_init(NULL,
                                ps_args(), TRUE,
                                "-hmm", _modelPath.c_str(),
                                "-lm", _lmPath.c_str(),
                                "-dict", _dictPath.c_str(),
                                "-logfn", _logPath.c_str(),
                                "-fsg", fsgname.c_str(),
//                          "-lw", "1.0",
//                          "-beam", "1e-80",
//                          "-wbeam", "1e-60",
//                          "-pbeam", "1e-80",
                                NULL);

        if (subConfig == NULL)
        {
            fprintf(stderr, "Failed to create config object, see log for details\n");
            return -1;
        }

        ps_reinit(_psWordDecoder, subConfig);

        if (_psWordDecoder == NULL)
        {
            fprintf(stderr, "Failed to create recognizer, see log for details\n");
            return -1;
        }

        long int dialogueLastsFor = (sub->getEndTime() - dialogueStartsAt);

        long int samplesAlreadyRead = dialogueStartsAt * 16;
        long int samplesToBeRead = dialogueLastsFor * 16;

        if((samplesAlreadyRead - recognitionWindow) >= 0)
            samplesAlreadyRead -= recognitionWindow;
        else
            samplesAlreadyRead = 0;

        if((samplesToBeRead + (2 * recognitionWindow)) < _samples.size())
            samplesToBeRead += (2 * recognitionWindow);

        else
            samplesToBeRead = _samples.size() - 1;

        const int16_t *sample = _samples.data();

        _rvWord = ps_start_utt(_psWordDecoder);
        _rvWord = ps_process_raw(_psWordDecoder, sample + samplesAlreadyRead, samplesToBeRead, FALSE, FALSE);
        _rvWord = ps_end_utt(_psWordDecoder);

        _hypWord = ps_get_hyp(_psWordDecoder, &_scoreWord);

        if (_hypWord == NULL)
        {
            _hypWord = "NULL";

            if(_parameters->displayRecognised)
            {
                std::cout << "\n\n-----------------------------------------\n\n";
                std::cout << "Recognised: " << _hypWord << "\n";
            }

            continue;

        }

        if(_parameters->displayRecognised)
        {
            std::cout << "\n\n-----------------------------------------\n\n";
            std::cout << "Start time of dialogue : " << dialogueStartsAt << "\n";
            std::cout << "End time of dialogue   : " << sub->getEndTime() << "\n\n";
            std::cout << "Recognised  : " << _hypWord << "\n";
            std::cout << "Actual      : " << sub->getDialogue() << "\n\n";
        }

        recognisedBlock currBlock = findAndSetWordTimes(subConfig, _psWordDecoder, sub);

        switch (_parameters->outputFormat)  //decide on basis of set output format
        {
            case srt:       subCount = printSRTContinuous(_outputFileName, subCount, sub, _parameters->printOption);
                break;

            case xml:       printXMLContinuous(_outputFileName, sub);
                break;

            case json:      printJSONContinuous(_outputFileName, sub);
                break;

            case karaoke:   subCount = printKaraokeContinuous(_outputFileName, subCount, sub, _parameters->printOption);
                break;

            default:        std::cout<<"An error occurred while choosing output format!";
                exit(2);
        }


        cmd_ln_free_r(subConfig);

        delete (currSub);

    }

    printFileEnd(_outputFileName, _parameters->outputFormat);

    return true;
}

bool PocketsphinxAligner::printAligned(std::string outputFileName, outputFormats format)
{
    switch (format)  //decide on basis of set output format
    {
        case srt:       printSRT(outputFileName, _subtitles, _parameters->printOption);
                        break;

        case xml:       printXML(outputFileName, _subtitles);
                        break;

        case json:      printJSON(outputFileName, _subtitles);
                        break;

        case karaoke:   printKaraoke(outputFileName, _subtitles, _parameters->printOption);
                        break;

        default:        FATAL(EXIT_FAILURE, "An error occurred while choosing output format!");
    }

    return true;
}


PocketsphinxAligner::~PocketsphinxAligner()
{

    ps_free(_psWordDecoder);
    cmd_ln_free_r(_configWord);

    if(_parameters->searchPhonemes)
    {
        ps_free(_psPhonemeDecoder);
        cmd_ln_free_r(_configPhoneme);
    }

    delete(_file);

    delete(_parser);
    delete(_subParserFactory);
    delete(_alignedData);
}
