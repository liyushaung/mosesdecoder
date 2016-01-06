#include "huffmanish.hh"

namespace Moses2
{

Huffman::Huffman (const char * filepath)
{
  //Read the file
  util::FilePiece filein(filepath);

  //Init uniq_lines to zero;
  uniq_lines = 0;

  line_text prev_line; //Check for unique lines.
  int num_lines = 0 ;

  while (true) {
    line_text new_line;

    num_lines++;

    try {
      //Process line read
      new_line = splitLine(filein.ReadLine());
      count_elements(new_line); //Counts the number of elements, adds new and increments counters.

    } catch (util::EndOfFileException e) {
      std::cerr << "Unique entries counted: ";
      break;
    }

    if (new_line.source_phrase == prev_line.source_phrase) {
      continue;
    } else {
      uniq_lines++;
      prev_line = new_line;
    }
  }

  std::cerr << uniq_lines << std::endl;
}

void Huffman::count_elements(const line_text &linein)
{
  //For target phrase:
  util::TokenIter<util::SingleCharacter> it(linein.target_phrase, util::SingleCharacter(' '));
  while (it) {
    //Check if we have that entry
    std::map<std::string, unsigned int>::iterator mapiter;
    mapiter = target_phrase_words.find(it->as_string());

    if (mapiter != target_phrase_words.end()) {
      //If the element is found, increment the count.
      mapiter->second++;
    } else {
      //Else create a new entry;
      target_phrase_words.insert(std::pair<std::string, unsigned int>(it->as_string(), 1));
    }
    it++;
  }

  //For word allignment 1
  std::map<std::vector<unsigned char>, unsigned int>::iterator mapiter3;
  std::vector<unsigned char> numbers = splitWordAll1(linein.word_align);
  mapiter3 = word_all1.find(numbers);

  if (mapiter3 != word_all1.end()) {
    //If the element is found, increment the count.
    mapiter3->second++;
  } else {
    //Else create a new entry;
    word_all1.insert(std::pair<std::vector<unsigned char>, unsigned int>(numbers, 1));
  }

}

//Assigns huffman values for each unique element
void Huffman::assign_values()
{
  //First create vectors for all maps so that we could sort them later.

  //Create a vector for target phrases
  for(std::map<std::string, unsigned int>::iterator it = target_phrase_words.begin(); it != target_phrase_words.end(); it++ ) {
    target_phrase_words_counts.push_back(*it);
  }
  //Sort it
  std::sort(target_phrase_words_counts.begin(), target_phrase_words_counts.end(), sort_pair());

  //Create a vector for word allignments 1
  for(std::map<std::vector<unsigned char>, unsigned int>::iterator it = word_all1.begin(); it != word_all1.end(); it++ ) {
    word_all1_counts.push_back(*it);
  }
  //Sort it
  std::sort(word_all1_counts.begin(), word_all1_counts.end(), sort_pair_vec());


  //Afterwards we assign a value for each phrase, starting from 1, as zero is reserved for delimiter
  unsigned int i = 1; //huffman code
  for(std::vector<std::pair<std::string, unsigned int> >::iterator it = target_phrase_words_counts.begin();
      it != target_phrase_words_counts.end(); it++) {
    target_phrase_huffman.insert(std::pair<std::string, unsigned int>(it->first, i));
    i++; //Go to the next huffman code
  }

  i = 1; //Reset i for the next map
  for(std::vector<std::pair<std::vector<unsigned char>, unsigned int> >::iterator it = word_all1_counts.begin();
      it != word_all1_counts.end(); it++) {
    word_all1_huffman.insert(std::pair<std::vector<unsigned char>, unsigned int>(it->first, i));
    i++; //Go to the next huffman code
  }

  //After lookups are produced, clear some memory usage of objects not needed anymore.
  target_phrase_words.clear();
  word_all1.clear();

  target_phrase_words_counts.clear();
  word_all1_counts.clear();

  std::cerr << "Finished generating huffman codes." << std::endl;

}

void Huffman::serialize_maps(const char * dirname)
{
  //Note that directory name should exist.
  std::string basedir(dirname);
  std::string target_phrase_path(basedir + "/target_phrases");
  std::string probabilities_path(basedir + "/probs");
  std::string word_all1_path(basedir + "/Wall1");

  //Target phrase
  std::ofstream os (target_phrase_path.c_str(), std::ios::binary);
  boost::archive::text_oarchive oarch(os);
  oarch << lookup_target_phrase;
  os.close();

  //Word all1
  std::ofstream os2 (word_all1_path.c_str(), std::ios::binary);
  boost::archive::text_oarchive oarch2(os2);
  oarch2 << lookup_word_all1;
  os2.close();
}

std::vector<unsigned char> Huffman::full_encode_line(const line_text &line)
{
  return vbyte_encode_line((encode_line(line)));
}

std::vector<unsigned int> Huffman::encode_line(const line_text &line)
{
  std::vector<unsigned int> retvector;

  //Get target_phrase first.
  util::TokenIter<util::SingleCharacter> it(line.target_phrase, util::SingleCharacter(' '));
  while (it) {
    retvector.push_back(target_phrase_huffman.find(it->as_string())->second);
    it++;
  }
  //Add a zero;
  retvector.push_back(0);

  //Get probabilities. Reinterpreting the float bytes as unsgined int.
  util::TokenIter<util::SingleCharacter> probit(line.prob, util::SingleCharacter(' '));
  while (probit) {
    //Sometimes we have too big floats to handle, so first convert to double
    double tempnum = atof(probit->data());
    float num = (float)tempnum;
    retvector.push_back(reinterpret_float(&num));
    probit++;
  }
  //Add a zero;
  retvector.push_back(0);


  //Get Word allignments
  retvector.push_back(word_all1_huffman.find(splitWordAll1(line.word_align))->second);
  retvector.push_back(0);

  //The rest of the components might not be there, but add them (as reinterpretation to byte arr)
  //In the future we should really make those optional to save space

  //Counts
  const char* counts = line.counts.data();
  size_t counts_size = line.counts.size();
  for (size_t i = 0; i < counts_size; i++) {
    retvector.push_back(counts[i]);
  }
  retvector.push_back(0);

  //Sparse score
  const char* sparse_score = line.sparse_score.data();
  size_t sparse_score_size = line.sparse_score.size();
  for (size_t i = 0; i < sparse_score_size; i++) {
    retvector.push_back(sparse_score[i]);
  }
  retvector.push_back(0);

  //Property
  const char* property = line.property.data();
  size_t property_size = line.property.size();
  for (size_t i = 0; i < property_size; i++) {
    retvector.push_back(property[i]);
  }
  retvector.push_back(0);

  return retvector;
}

void Huffman::produce_lookups()
{
  //basically invert every map that we have
  for(std::map<std::string, unsigned int>::iterator it = target_phrase_huffman.begin(); it != target_phrase_huffman.end(); it++ ) {
    lookup_target_phrase.insert(std::pair<unsigned int, std::string>(it->second, it->first));
  }

  for(std::map<std::vector<unsigned char>, unsigned int>::iterator it = word_all1_huffman.begin(); it != word_all1_huffman.end(); it++ ) {
    lookup_word_all1.insert(std::pair<unsigned int, std::vector<unsigned char> >(it->second, it->first));
  }

}

HuffmanDecoder::HuffmanDecoder (const char * dirname)
{
  //Read the maps from disk

  //Note that directory name should exist.
  std::string basedir(dirname);
  std::string target_phrase_path(basedir + "/target_phrases");
  std::string word_all1_path(basedir + "/Wall1");

  //Target phrases
  std::ifstream is (target_phrase_path.c_str(), std::ios::binary);
  boost::archive::text_iarchive iarch(is);
  iarch >> lookup_target_phrase;
  is.close();

  //Word allignment 1
  std::ifstream is2 (word_all1_path.c_str(), std::ios::binary);
  boost::archive::text_iarchive iarch2(is2);
  iarch2 >> lookup_word_all1;
  is2.close();

}

HuffmanDecoder::HuffmanDecoder (const std::map<unsigned int, std::string> &lookup_target,
                                const std::map<unsigned int, std::vector<unsigned char> > &lookup_word1)
{
  lookup_target_phrase = lookup_target;
  lookup_word_all1 = lookup_word1;
}

std::vector<target_text*> HuffmanDecoder::full_decode_line (unsigned char lines[],
		size_t linesCount,
		int num_scores,
		int num_lex_scores,
		RecycleData &recycler)
{
  std::vector<target_text*> retvector; //All target phrases
  std::vector<unsigned int> *decoded_lines = vbyte_decode_line(lines, linesCount, recycler); //All decoded lines
  std::vector<unsigned int>::iterator it = decoded_lines->begin(); //Iterator for them
  std::vector<unsigned int> current_target_phrase; //Current target phrase decoded

  short zero_count = 0; //Count how many zeroes we have met. so far. Every 3 zeroes mean a new target phrase.
  while(it != decoded_lines->end()) {
    if (zero_count == 1) {
      //We are extracting scores. we know how many scores there are so we can push them
      //to the vector. This is done in case any of the scores is 0, because it would mess
      //up the state machine.
      for (int i = 0; i < num_scores + num_lex_scores; i++) {
        current_target_phrase.push_back(*it);
        it++;
      }
    }

    if (zero_count == 6) {
      //We have finished with this entry, decode it, and add it to the retvector.
      retvector.push_back(decode_line(current_target_phrase, num_scores, num_lex_scores, recycler));
      current_target_phrase.clear(); //Clear the current target phrase and the zero_count
      zero_count = 0; //So that we can reuse them for the next target phrase
    }
    //Add to the next target_phrase, number by number.
    current_target_phrase.push_back(*it);
    if (*it == 0) {
      zero_count++;
    }
    it++; //Go to the next word/symbol
  }
  //Don't forget the last remaining line!
  if (zero_count == 6) {
    //We have finished with this entry, decode it, and add it to the retvector.
    retvector.push_back(decode_line(current_target_phrase, num_scores, num_lex_scores, recycler));
    current_target_phrase.clear(); //Clear the current target phrase and the zero_count
    zero_count = 0; //So that we can reuse them for the next target phrase
  }

  recycler.huffman_line.push_back(decoded_lines);

  return retvector;

}

target_text *HuffmanDecoder::decode_line (const std::vector<unsigned int> &input,
		int num_scores,
		int num_lex_scores,
		Moses2::RecycleData &recycler)
{
  //demo decoder
  target_text *ret;
  if (recycler.tt.empty()) {
	  ret = new target_text;
  }
  else {
	  ret = recycler.tt.back();
	  recycler.tt.pop_back();

	  ret->Reset();
  }

  ret->prob.reserve(num_scores);
  //Split everything
  unsigned int wAll;

  //Split the line into the proper arrays
  short num_zeroes = 0;
  int counter = 0;
  while (num_zeroes < 6) {
    unsigned int num = input[counter];
    if (num == 0) {
      num_zeroes++;
    } else if (num_zeroes == 0) {
      ret->target_phrase.push_back(num);
    } else if (num_zeroes == 1) {
      //Push exactly num_scores scores
      for (int i = 0; i < num_scores + num_lex_scores; i++) {
    	float prob = reinterpret_uint(&num);
    	ret->prob.push_back(prob);

        counter++;
        num = input[counter];
      }
      continue;
    } else if (num_zeroes == 2) {
      wAll = num;
    } else if (num_zeroes == 3) {
      ret->counts.push_back(static_cast<char>(input[counter]));
    } else if (num_zeroes == 4) {
      ret->sparse_score.push_back(static_cast<char>(input[counter]));
    } else if (num_zeroes == 5) {
      ret->property.push_back(static_cast<char>(input[counter]));
    }

    counter++;
  }

  ret->word_all1 = lookup_word_all1.find(wAll)->second;

  return ret;

}

inline const std::string &HuffmanDecoder::getTargetWordFromID(unsigned int id)
{
  return lookup_target_phrase.find(id)->second;
}

std::string HuffmanDecoder::getTargetWordsFromIDs(const std::vector<unsigned int> &ids)
{
  std::string returnstring;
  for (std::vector<unsigned int>::const_iterator it = ids.begin(); it != ids.end(); it++) {
    returnstring.append(getTargetWordFromID(*it) + " ");
  }

  return returnstring;
}

inline const std::string &getTargetWordFromID(unsigned int id, const std::map<unsigned int, std::string> &lookup_target_phrase)
{
  return lookup_target_phrase.find(id)->second;
}

std::string getTargetWordsFromIDs(const std::vector<unsigned int> &ids, const std::map<unsigned int, std::string> &lookup_target_phrase)
{
  std::string returnstring;
  for (std::vector<unsigned int>::const_iterator it = ids.begin(); it != ids.end(); it++) {
    returnstring.append(getTargetWordFromID(*it, lookup_target_phrase) + " ");
  }

  return returnstring;
}

/*Those functions are used to more easily store the floats in the binary phrase table
 We convert the float unsinged int so that it is the same as our other values and we can
 apply variable byte encoding on top of it.*/

inline unsigned int reinterpret_float(float * num)
{
  unsigned int * converted_num;
  converted_num = reinterpret_cast<unsigned int *>(num);
  return *converted_num;
}

inline float reinterpret_uint(unsigned int * num)
{
  float * converted_num;
  converted_num = reinterpret_cast<float *>(num);
  return *converted_num;
}

/*Mostly taken from stackoverflow, http://stackoverflow.com/questions/5858646/optimizing-variable-length-encoding
and modified in order to return a vector of chars. Implements ULEB128 or variable byte encoding.
This is highly optimized version with unrolled loop */
inline std::vector<unsigned char> vbyte_encode(unsigned int num)
{
  //Determine how many bytes we are going to take.
  short size;
  std::vector<unsigned char> byte_vector;

  if (num < 0x00000080U) {
    size = 1;
    byte_vector.reserve(size);
    goto b1;
  }
  if (num < 0x00004000U) {
    size = 2;
    byte_vector.reserve(size);
    goto b2;
  }
  if (num < 0x00200000U) {
    size = 3;
    byte_vector.reserve(size);
    goto b3;
  }
  if (num < 0x10000000U) {
    size = 4;
    byte_vector.reserve(size);
    goto b4;
  }
  size = 5;
  byte_vector.reserve(size);


  //Now proceed with the encoding.
  byte_vector.push_back((num & 0x7f) | 0x80);
  num >>= 7;
b4:
  byte_vector.push_back((num & 0x7f) | 0x80);
  num >>= 7;
b3:
  byte_vector.push_back((num & 0x7f) | 0x80);
  num >>= 7;
b2:
  byte_vector.push_back((num & 0x7f) | 0x80);
  num >>= 7;
b1:
  byte_vector.push_back(num);

  return byte_vector;
}

std::vector<unsigned int> *vbyte_decode_line(unsigned char line[], size_t linesSize, RecycleData &recycler)
{
  std::vector<unsigned int> *huffman_line;
  if (recycler.huffman_line.empty()) {
	  huffman_line = new std::vector<unsigned int>();
  }
  else {
	  huffman_line = recycler.huffman_line.back();
	  recycler.huffman_line.pop_back();
	  huffman_line->clear();
  }

  unsigned char current_num[linesSize];

  size_t current_num_ind = 0;
  for (size_t i = 0; i < linesSize; ++i) {
	unsigned char c = line[i];
    current_num[current_num_ind++] = c;
    if ((c >> 7) != 1) {
      //We don't have continuation in the next bit
      huffman_line->push_back(bytes_to_int(current_num, current_num_ind));
      current_num_ind = 0;
    }
  }
  return huffman_line;
}

inline unsigned int bytes_to_int(unsigned char number[], size_t numberSize)
{
  unsigned int retvalue = 0;
  unsigned char shift = 0; //By how many bits to shift

  for (size_t i = 0; i < numberSize; ++i) {
	unsigned char c = number[i];
    retvalue |= (c & 0x7f) << shift;
    shift += 7;
  }

  return retvalue;
}

std::vector<unsigned char> vbyte_encode_line(const std::vector<unsigned int> &line)
{
  std::vector<unsigned char> retvec;

  //For each unsigned int in the line, vbyte encode it and add it to a vector of unsigned chars.
  for (std::vector<unsigned int>::const_iterator it = line.begin(); it != line.end(); it++) {
    std::vector<unsigned char> vbyte_encoded = vbyte_encode(*it);
    retvec.insert(retvec.end(), vbyte_encoded.begin(), vbyte_encoded.end());
  }

  return retvec;
}

}
