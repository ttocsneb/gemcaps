use std::{io::{Read, self, Seek}, mem};

pub struct Buffer {
    pub buf: Vec<u8>,
    index: usize,
}

impl Buffer {
    pub fn new() -> Self {
        Self { buf: Vec::new(), index: 0 }
    }
    
    pub fn with_capacity(capacity: usize) -> Self {
        Self { buf: Vec::with_capacity(capacity), index: 0 }
    }

    pub fn ready(&self) -> usize {
        self.buf.len() - self.index
    }

}

impl Read for Buffer {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let ready = self.ready();
        if ready == 0 {
            return Ok(0);
        }
        
        let stop = if buf.len() > ready { ready } else { buf.len() };

        let new_buf = &self.buf[self.index..self.index + stop];
        for (b, new_b) in buf.into_iter().zip(new_buf) {
            drop(mem::replace(b, *new_b));
        }
        
        self.index += new_buf.len();
        Ok(new_buf.len())
    }
}

impl Seek for Buffer {
    fn seek(&mut self, pos: io::SeekFrom) -> io::Result<u64> {
        let index = match pos {
            io::SeekFrom::Start(offset) => offset as isize,
            io::SeekFrom::End(offset) => (offset + self.buf.len() as i64) as isize,
            io::SeekFrom::Current(offset) => (offset + self.index as i64) as isize,
        };
        if index < 0 {
            return Err(io::Error::new(io::ErrorKind::InvalidInput, "Cannot seek below 0"));
        } else if index as usize > self.buf.len() {
            self.index = self.buf.len();
        } else {
            self.index = index as usize;
        }
        Ok(self.index as u64)
    }
}