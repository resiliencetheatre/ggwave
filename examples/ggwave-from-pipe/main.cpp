/* 
 * 
 * Work In Progress: ggwave-to-file -> ggwave-from-pipe 
 * 
 * Target: We should read pipe and play that out
 *
 *  
 */

#include "ggwave/ggwave.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ggwave-common.h"

#include <cstdio>
#include <cstring>
#include <iostream>

// fifo
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>


#define FIFO_NAME "/tmp/fifo_in"

int main(int argc, char** argv) {
	
	struct stat st;

    // Check if FIFO already exists and is a named pipe
    if (stat(FIFO_NAME, &st) == 0) {
        if (!S_ISFIFO(st.st_mode)) {
            std::cerr << "Error: " << FIFO_NAME << " exists but is not a FIFO!\n";
            return 1;
        }
    } else {
        // FIFO does not exist, create it
        if (mkfifo(FIFO_NAME, 0666) == -1) {
            std::cerr << "Error creating FIFO!\n";
            return 1;
        }
    }
    



    #if defined(_WIN32)
    const std::string & defaultFile = "audio.wav";
    #else
    const std::string & defaultFile = "/dev/stdout";
    #endif

    fprintf(stderr, "Usage: %s [-vN] [-sN] [-pN] [-lN] [-d]\n", argv[0]);
    fprintf(stderr, "    -fF - output filename, (default: %s)\n", defaultFile.c_str());
    fprintf(stderr, "    -vN - output volume, N in (0, 100], (default: 50)\n");
    fprintf(stderr, "    -sN - output sample rate, N in [%d, %d], (default: %d)\n", (int) GGWave::kSampleRateMin, (int) GGWave::kSampleRateMax, (int) GGWave::kDefaultSampleRate);
    fprintf(stderr, "    -pN - select the transmission protocol id (default: 1)\n");
    fprintf(stderr, "    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    fprintf(stderr, "    -d  - use Direct Sequence Spread (DSS)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    Available protocols:\n");

    const auto & protocols = GGWave::Protocols::kDefault();
    for (int i = 0; i < (int) protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        fprintf(stderr, "      %d - %s\n", i, protocol.name);
    }
    fprintf(stderr, "\n");

    if (argc < 1) {
        return -1;
    }

    const auto argm = parseCmdArguments(argc, argv);

    if (argm.count("h") > 0) {
        return 0;
    }

    const int   volume        = argm.count("v") == 0 ? 50 : std::stoi(argm.at("v"));
    const std::string & file  = argm.count("f") == 0 ? defaultFile : argm.at("f");
    const float sampleRateOut = argm.count("s") == 0 ? GGWave::kDefaultSampleRate : std::stof(argm.at("s"));
    const int   protocolId    = argm.count("p") == 0 ?  1 : std::stoi(argm.at("p"));
    const int   payloadLength = argm.count("l") == 0 ? -1 : std::stoi(argm.at("l"));
    const bool  useDSS        = argm.count("d") >  0;

    if (volume <= 0 || volume > 100) {
        fprintf(stderr, "Invalid volume\n");
        return -1;
    }

    if (sampleRateOut < GGWave::kSampleRateMin || sampleRateOut > GGWave::kSampleRateMax) {
        fprintf(stderr, "Invalid sample rate: %g\n", sampleRateOut);
        return -1;
    }

    if (protocolId < 0 || protocolId >= (int) protocols.size()) {
        fprintf(stderr, "Invalid transmission protocol id\n");
        return -1;
    }

    if (protocols[protocolId].enabled == false) {
        fprintf(stderr, "Protocol %d is not enabled\n", protocolId);
        return -1;
    }

    // fprintf(stderr, "Enter a text message:\n");
	// Get payload from input
    std::string message;
    // std::getline(std::cin, message);


	fprintf(stderr, "waiting for fifo input ... (/tmp/fifo_in) \n");

	// Get payload from FIFO
	
	// Open FIFO for reading
    int fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error opening FIFO for reading\n";
        return 1;
    }

    char buffer[100];
    
	

    // Read from FIFO
    int bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Null-terminate the string
        std::cout << "Received: " << buffer << std::endl;
    }

    // Close FIFO
    close(fd);
    
    // Copy buffer to message
    message = std::string(buffer, bytesRead);










    if (message.size() == 0) {
        fprintf(stderr, "Invalid message: size = 0\n");
        return -2;
    }

    if (message.size() > 140) {
        fprintf(stderr, "Invalid message: size > 140\n");
        return -3;
    }

    fprintf(stderr, "Generating waveform for message '%s' ...\n", message.c_str());

    GGWave::OperatingMode mode = GGWAVE_OPERATING_MODE_RX_AND_TX;
    if (useDSS) mode |= GGWAVE_OPERATING_MODE_USE_DSS;

    GGWave ggWave({
        payloadLength,
        GGWave::kDefaultSampleRate,
        sampleRateOut,
        GGWave::kDefaultSampleRate,
        GGWave::kDefaultSamplesPerFrame,
        GGWave::kDefaultSoundMarkerThreshold,
        GGWAVE_SAMPLE_FORMAT_F32,
        GGWAVE_SAMPLE_FORMAT_I16,
        mode,
    });
    ggWave.init(message.size(), message.data(), GGWave::TxProtocolId(protocolId), volume);

    const auto nBytes = ggWave.encode();
    if (nBytes == 0) {
        fprintf(stderr, "Failed to generate waveform!\n");
        return -4;
    }

    std::vector<char> bufferPCM(nBytes);
    std::memcpy(bufferPCM.data(), ggWave.txWaveform(), nBytes);

    fprintf(stderr, "Output file = %s\n", file.c_str());
    fprintf(stderr, "Output size = %d bytes\n", (int) bufferPCM.size());

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = sampleRateOut;
    format.bitsPerSample = 16;

	// Write WAV
    fprintf(stderr, "Writing WAV data ...\n");

    drwav wav;
    drwav_init_file_write(&wav, file.c_str(), &format, NULL);
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, bufferPCM.size()/2, bufferPCM.data());

    fprintf(stderr, "WAV frames written = %d\n", (int) framesWritten);

    drwav_uninit(&wav);

    return 0;
}
