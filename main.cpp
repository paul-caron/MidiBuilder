#include <iostream>
#include <vector>
#include <fstream>
#include <ctime>
#include <array>


std::array<unsigned char, 4> uintTo7bits(unsigned int n){
  std::array<unsigned char, 4> result{};
  result.at(3) = (n & 0b00000000000000000000000001111111)      ;
  result.at(2) = (n & 0b00000000000000000011111110000000) >> 7 ;
  result.at(1) = (n & 0b00000000000111111100000000000000) >> 14;
  result.at(0) = (n & 0b00001111111000000000000000000000) >> 21;
  return result;
}


class MidiBuilder{

std::string filename{};
std::ofstream midiFile{};
std::vector<char> midiBuffer{};

//midi header info
unsigned short fileType{};
unsigned short numberOfTracks{};
unsigned short divisionTime{};

public:
  MidiBuilder(){
    askType();
    askTracks();
    askTime();
    buildMidiHeader();
    buildTracks();
    saveMidiFile();
  }
  void saveMidiFile(){
    std::cout << "Save As?" << std::endl
              << "  > ";
    std::cin >> filename;
    midiFile.open(filename, std::ios::binary);
    for(auto c: midiBuffer){
      midiFile.write(reinterpret_cast<char *>(&c), 1);
    }
  }
  void askTime(){
    std::cout << "How many ticks per quarter note?" << std::endl
              << "  > ";
    std::cin >> divisionTime;
  }
  void askType(){
    std::cout << "What type of MIDI file would you like to create?"
              << std::endl
              << "  0 = 1 track " << std::endl
              << "  1 = Multiple parallel tracks" << std::endl
              << "  2 = Multiple sequential tracks" << std::endl
              << "  > " ;
    std::cin >> fileType;
    while(fileType > 2){
      std::cout << "Enter a valid MIDI file type (0, 1 or 2)"
                << std::endl;
      std::cin >> fileType;
    }
  }
  void askTracks(){
    std::cout << "How many tracks will this file contain?" << std::endl
              << "  > ";
    std::cin >> numberOfTracks;
  }
  void buildMidiHeader(){
    //header chunk magick
    for(const auto c: {'M','T','h','d'})
      midiBuffer.push_back(c);

    //header chunk length, always 6
    for(const auto c: {0x00,0x00,0x00,0x06})
      midiBuffer.push_back(c);

    //midi file type
    unsigned char high = (fileType & 0xFF00) >> 8 ;
    unsigned char low = fileType & 0x00FF ;
    midiBuffer.push_back(high);
    midiBuffer.push_back(low);

    //midi file number of tracks
    high = (numberOfTracks & 0xFF00) >> 8 ;
    low = numberOfTracks & 0x00FF ;
    midiBuffer.push_back(high);
    midiBuffer.push_back(low);

    //midi file division time
    high = (divisionTime & 0xFF00) >> 8 ;
    low = divisionTime & 0x00FF ;
    midiBuffer.push_back(high);
    midiBuffer.push_back(low);
  }
  void buildTracks(){
    for(unsigned short i=0;i<numberOfTracks;i++){
      std::cout << "Building Track " << i << std::endl;
      buildTrack();
    }
  }
  void buildTrack(){
    unsigned char programNumber{}; //0-127
    unsigned char channelNumber{}; //0-15
    std::cout << "What is the program/instrument number? (0-127)" << std::endl << "  > ";
    unsigned pn{};
    std::cin >> pn;
    while(pn > 127){
      std::cout << "Enter a valid program number (0-127)" << std::endl
                << "  > ";
      std::cin >> pn;
    }
    programNumber = pn & 0x000000ff;
    std::cout << "What is the channel number?" << std::endl << "  > ";
    unsigned cn{};
    std::cin >> cn;
    while(cn > 15){
      std::cout << "Enter a valid channel number (0-15)" << std::endl
                << "  > ";
      std::cin >> cn;
    }
    channelNumber = cn & 0x000000ff;
    std::vector<unsigned char> trackBuffer{};

    //header chunk magick
    for(const auto c: {'M','T','r','k'})
      trackBuffer.push_back(c);

    //header chunk length, leave at zero value for now but will change later
    for(const auto c: {0x00,0x00,0x00,0x00})
      trackBuffer.push_back(c);

    //channel prefix
    for(const auto c: {0x00, 0xff, 0x20, 0x01})
      trackBuffer.push_back(c);
    trackBuffer.push_back(channelNumber);

    //program change
    for(const auto c: {0x00, 0xc0 + channelNumber})
      trackBuffer.push_back(c);
    trackBuffer.push_back(programNumber);

    //events
    unsigned eventSelection{};
    do{
      eventSelection = 0;
      std::cout << "Would you like to add an event?" << std::endl
                << "  0 - No. (End the Track)" << std::endl
                << "  1 - One note" << std::endl
                << "  2 - Two simultaneous notes" << std::endl
                << "  3 - Three simultaneous notes" << std::endl
                << "  4 - Four simultaneous notes" << std::endl
                << "  9 - Arpeggio" << std::endl
                << "  > ";
      std::cin >> eventSelection;
      switch(eventSelection){
        case 1: buildNotes(trackBuffer, channelNumber, 1);break;
        case 2: buildNotes(trackBuffer, channelNumber, 2);break;
        case 3: buildNotes(trackBuffer, channelNumber, 3);break;
        case 4: buildNotes(trackBuffer, channelNumber, 4);break;
        case 9: arpeggio(trackBuffer, channelNumber);break;
        default: ;
      }
    }while(eventSelection);

    //end of track
    for(const auto c: {0x00, 0xff, 0x2f, 0x00})
      trackBuffer.push_back(c);
    std::cout << "End of track" << std::endl;

    //adjust length of track (byte 4-7 in the buffer)
    unsigned int trackLength = trackBuffer.size() - 8;
    trackBuffer.at(4) = (trackLength & 0xff000000) >> 24;
    trackBuffer.at(5) = (trackLength & 0x00ff0000) >> 16;
    trackBuffer.at(6) = (trackLength & 0x0000ff00) >> 8;
    trackBuffer.at(7) = (trackLength & 0x000000ff) ;

    //load track buffer into midibuffer
    for(auto c: trackBuffer)
      midiBuffer.push_back(c);
  }
  void buildNotes(std::vector<unsigned char> & trackBuffer, unsigned char channelNumber, unsigned numberOfNotes){
    unsigned deltaTime{};
    unsigned velocity{};
    unsigned duration{};

    std::cout << "What is the delta time?" << std::endl
              << "  > ";
    std::cin >> deltaTime;
    std::cout << "What is the duration?" << std::endl
              << "  > ";
    std::cin >> duration;
    std::cout << "What is the velocity?(0-127)" << std::endl
              << "  > ";
    std::cin >> velocity;


    std::vector<unsigned char> noteNumbers{};

    for(unsigned i=0; i<numberOfNotes; i++)
    {
      unsigned noteNumber{};
      std::cout << i
                << " - What is the note number?(0-127)" << std::endl
                << "  > ";
      std::cin >> noteNumber;
      noteNumbers.push_back(noteNumber);
    }

    writeSimultaneousNotes(
      trackBuffer,
      channelNumber,
      deltaTime,
      duration,
      velocity,
      noteNumbers);

  }
  void writeSimultaneousNotes(
    std::vector<unsigned char> & trackBuffer,
    unsigned char channelNumber,
    unsigned deltaTime,
    unsigned duration,
    unsigned velocity,
    std::vector<unsigned char> noteNumbers)
  {
    std::array<unsigned char, 4> deltaTimeArr = uintTo7bits(deltaTime);
    std::array<unsigned char, 4> durationArr = uintTo7bits(duration);
    for(auto noteNumber: noteNumbers){
      if(deltaTimeArr.at(0)){
      trackBuffer.push_back(deltaTimeArr.at(0) + 0b10000000);
      trackBuffer.push_back(deltaTimeArr.at(1) + 0b10000000);
      trackBuffer.push_back(deltaTimeArr.at(2) + 0b10000000);
      trackBuffer.push_back(deltaTimeArr.at(3));
      }
      else if(deltaTimeArr.at(1)){
      trackBuffer.push_back(deltaTimeArr.at(1) + 0b10000000);
      trackBuffer.push_back(deltaTimeArr.at(2) + 0b10000000);
      trackBuffer.push_back(deltaTimeArr.at(3));
      }
      else if(deltaTimeArr.at(2)){
      trackBuffer.push_back(deltaTimeArr.at(2) + 0b10000000);
      trackBuffer.push_back(deltaTimeArr.at(3));
      }
      else{
      trackBuffer.push_back(deltaTimeArr.at(3));
      }
      trackBuffer.push_back(0x90 + channelNumber);
      trackBuffer.push_back((char) noteNumber);
      trackBuffer.push_back((char) velocity);
    }

    bool first = true;
    for(auto noteNumber: noteNumbers){
      if(!first) durationArr = std::array<unsigned char,4>{0x00,0x00,0x00,0x00};
      if(durationArr.at(0)){
      trackBuffer.push_back(durationArr.at(0) + 0b10000000);
      trackBuffer.push_back(durationArr.at(1) + 0b10000000);
      trackBuffer.push_back(durationArr.at(2) + 0b10000000);
      trackBuffer.push_back(durationArr.at(3));
      }
      else if(durationArr.at(1)){
      trackBuffer.push_back(durationArr.at(1) + 0b10000000);
      trackBuffer.push_back(durationArr.at(2) + 0b10000000);
      trackBuffer.push_back(durationArr.at(3));
      }
      else if(durationArr.at(2)){
      trackBuffer.push_back(durationArr.at(2) + 0b10000000);
      trackBuffer.push_back(durationArr.at(3));
      }
      else{
      trackBuffer.push_back(durationArr.at(3));
      }
      trackBuffer.push_back(0x90 + channelNumber);
      trackBuffer.push_back((char) noteNumber);
      trackBuffer.push_back(0x00);
      first = false;
    }
  }
  void arpeggio(std::vector<unsigned char> & trackBuffer, unsigned char channelNumber){
    unsigned numberOfNoteNumbers;
    unsigned numberOfNotesToPlay;
    unsigned arpMode; //order vs random
    unsigned deltaTime{};
    unsigned velocity{};
    unsigned duration{};

    std::cout << "What is the delta time?" << std::endl
              << "  > ";
    std::cin >> deltaTime;
    std::cout << "What is the duration?" << std::endl
              << "  > ";
    std::cin >> duration;
    std::cout << "What is the velocity?(0-127)" << std::endl
              << "  > ";
    std::cin >> velocity;
    std::cout << "How many notes to play?" << std::endl
              << "  > ";
    std::cin >> numberOfNotesToPlay;
    std::cout << "How many note numbers?" << std::endl
              << "  > ";
    std::cin >> numberOfNoteNumbers;
    std::cout << "How to arpeggiate the notes?" << std::endl
              << "  0 - In order" << std::endl
              << "  1 - In random order" << std::endl
              << "  > ";
    std::cin >> arpMode;

    std::vector<unsigned char> noteNumbers{};

    for(unsigned i=0; i<numberOfNoteNumbers; i++)
    {
      unsigned noteNumber{};
      std::cout << i
                << " - What is the note number?(0-127)" << std::endl
                << "  > ";
      std::cin >> noteNumber;
      noteNumbers.push_back(noteNumber);
    }


    std::vector<unsigned char> sequence{};

    if(arpMode == 0){
      auto iter = noteNumbers.begin();
      for(unsigned i=0; i<numberOfNotesToPlay; i++){
        sequence.push_back(*iter);
        iter++;
        if(iter == noteNumbers.end()) iter = noteNumbers.begin();
      }
    }
    else if(arpMode == 1){
      for(unsigned i=0; i<numberOfNotesToPlay; i++){
        sequence.push_back(noteNumbers.at(rand()%noteNumbers.size()));
      }
    }
    for(auto noteNumber: sequence){
      writeSimultaneousNotes(
        trackBuffer,
        channelNumber,
        deltaTime,
        duration,
        velocity,
        {noteNumber});
    }

  }
};


int main(int argc, char ** argv){
  srand(time(0));
  MidiBuilder midiBuilder;
  return 0;
}



