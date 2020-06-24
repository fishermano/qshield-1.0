#include "QFlatbuffersReaders.h"

void QEncryptedTokenToQTokenReader::reset(const qix::QEncryptedToken *encrypted_token){
  const size_t tk_len = dec_size(encrypted_token->enc_tk()->size());
  tk_buf.reset(new uint8_t[tk_len]);
  tk_decrypt(encrypted_token->enc_tk()->data(), encrypted_token->enc_tk()->size(), tk_buf.get());
  BufferRefView<qix::QToken> buf(tk_buf.get(), tk_len);
  buf.verify();

  tk = buf.root();
  initialized = true;
}

QTokenReader::QTokenReader(BufferRefView<qix::QEncryptedToken> buf){
  buf.verify();
  reset(buf.root());
}

void QTokenReader::reset(const qix::QEncryptedToken *encrypted_token){
  this->encrypted_token = encrypted_token;
  init_enc_tk_reader();
}

void QTokenReader::init_enc_tk_reader(){
  enc_tk_reader.reset(encrypted_token);
}

uint32_t QTokenReader::w(){
  return enc_tk_reader.get_token()->w();
}

uint32_t QTokenReader::c(){
  return enc_tk_reader.get_token()->c();
}

void QTokenReader::sk_b(uint8_t **data, uint32_t *size){
  *data = (uint8_t *)enc_tk_reader.get_token()->sk_b()->data();
  *size = enc_tk_reader.get_token()->sk_b()->size();
}

void QBlockToQRowReader::reset(const qix::QBlock *block){
  uint32_t num_rows = block->num_rows();
  rows = block->rows();
  if(rows->rows()->size() != num_rows) {
    throw std::runtime_error(
      std::string("QBlock claimed to contain ")
      + std::to_string(num_rows)
      + std::string("rows but actually contains ")
      + std::to_string(rows->rows()->size())
      + std::string(" rows"));
  }

  row_idx = 0;
  initialized = true;
}

QRowReader::QRowReader(BufferRefView<qix::QEncryptedBlocks> buf){
  reset(buf);
}

QRowReader::QRowReader(const qix::QEncryptedBlocks *encrypted_blocks){
  reset(encrypted_blocks);
}

void QRowReader::reset(BufferRefView<qix::QEncryptedBlocks> buf){
  buf.verify();
  reset(buf.root());
}

void QRowReader::reset(const qix::QEncryptedBlocks *encrypted_blocks){
  if(encrypted_blocks->enc_blocks()->size() != 0){
    const size_t blocks_len = dec_size(encrypted_blocks->enc_blocks()->size());

    blocks_buf.reset(new uint8_t[blocks_len]);
    rdd_decrypt(encrypted_blocks->enc_blocks()->data(),
              encrypted_blocks->enc_blocks()->size(),
              blocks_buf.get());
    BufferRefView<qix::QBlocks> buf(blocks_buf.get(), blocks_len);
    buf.verify();
    blocks = buf.root();
    block_idx = 0;
    init_block_reader();
  }else{
    blocks_buf = nullptr;
    blocks = nullptr;
    block_idx = 0;
  }
}

void QRowReader::init_block_reader(){
  if (block_idx < blocks->blocks()->size()) {
    block_reader.reset(blocks->blocks()->Get(block_idx));
  }
}

uint32_t QRowReader::num_rows() {
  uint32_t result = 0;
  for (auto it = blocks->blocks()->begin();
        it != blocks->blocks()->end(); ++it) {
      result += it->num_rows();
  }
  return result;
}

bool QRowReader::has_next() {
    return block_reader.has_next() || (blocks != nullptr && block_idx + 1 < blocks->blocks()->size());
}

const tuix::Row *QRowReader::next() {
  if (!block_reader.has_next()) {
    assert((block_idx+1) < blocks->blocks()->size());
    block_idx++;
    init_block_reader();
  }

  return block_reader.next();
}

const qix::QMeta *QRowReader::meta(){
  return blocks->meta();
}

QSortedRunsReader::QSortedRunsReader(BufferRefView<qix::QSortedRuns> buf){
  reset(buf);
}

void QSortedRunsReader::reset(BufferRefView<qix::QSortedRuns> buf){
  buf.verify();
  sorted_runs = buf.root();
  run_readers.clear();
  for(auto it = sorted_runs->runs()->begin(); it != sorted_runs->runs()->end(); ++it){
    run_readers.push_back(QRowReader(*it));
  }
}

uint32_t QSortedRunsReader::num_runs(){
  return sorted_runs->runs()->size();
}

bool QSortedRunsReader::run_has_next(uint32_t run_idx){
  return run_readers[run_idx].has_next();
}

const tuix::Row *QSortedRunsReader::next_from_run(uint32_t run_idx){
  return run_readers[run_idx].next();
}

const qix::QMeta *QSortedRunsReader::meta(){
  return run_readers[0].meta();
}


QEncryptedBlocksToQBlockReader::QEncryptedBlocksToQBlockReader(BufferRefView<qix::QEncryptedBlocks> buf){
  buf.verify();
  const qix::QEncryptedBlocks *encrypted_blocks = buf.root();
  const size_t blocks_len = dec_size(encrypted_blocks->enc_blocks()->size());
  blocks_buf.reset(new uint8_t[blocks_len]);
  rdd_decrypt(encrypted_blocks->enc_blocks()->data(),
            encrypted_blocks->enc_blocks()->size(),
            blocks_buf.get());
  BufferRefView<qix::QBlocks> buf2(blocks_buf.get(), blocks_len);
  buf2.verify();
  blocks = buf2.root();
}
