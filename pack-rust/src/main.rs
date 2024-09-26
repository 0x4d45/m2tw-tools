use binread::BinReaderExt;
use binread::NullString;
use clap::Args;
use clap::Parser;
use clap::Subcommand;
use log::debug;
use log::error;
use log::info;
use std::io::BufReader;
use std::io::BufWriter;
use std::io::Read;
use std::io::Seek;
use std::io::Write;
use std::path::Path;
use std::path::PathBuf;

// ---------------------------------------------------------

/// This program manipulates Total War: Medieval II .pack files.
#[derive(Debug, Parser)]
#[clap(name = "pack", version)]
struct App {
    #[clap(subcommand)]
    command: Command,
    /// Verbosity level
    #[arg(long, global = true, value_name = "LEVEL", default_value = "3")]
    verbosity: usize,
}

#[derive(Debug, Subcommand)]
enum Command {
    /// Extract files from pack
    Extract(ExtractArgs),
    /// List files in pack
    List(ListArgs),
}

#[derive(Debug, Args)]
struct ExtractArgs {
    /// Output directory
    #[arg(long, default_value = ".")]
    dest: PathBuf,
    /// Pattern for files to be extracted
    #[arg(long, value_name = "GLOB")]
    filter: Option<String>,
    /// Pack files to unpack
    #[arg(value_name = "PACK", required = true)]
    packs: Vec<PathBuf>,
}

#[derive(Debug, Args)]
struct ListArgs {
    /// Pack files to list
    #[arg(value_name = "PACK", required = true)]
    packs: Vec<PathBuf>,
}

fn main() {
    let args = App::parse();

    stderrlog::new()
        .verbosity(args.verbosity)
        .module(module_path!())
        .init()
        .unwrap();

    match args.command {
        Command::Extract(args) => match cmd_extract(&args) {
            Err(error) => {
                error!("{}", error.to_string());
                std::process::exit(1);
            }
            _ => {}
        },
        Command::List(args) => match cmd_list(&args) {
            Err(error) => {
                error!("{}", error.to_string());
                std::process::exit(1);
            }
            _ => {}
        },
    }
}

// ---------------------------------------------------------

const LZO_BUFFER_SIZE: u32 = 65536;

fn cmd_extract(args: &ExtractArgs) -> Result<(), String> {
    let execution_timer = std::time::Instant::now();

    let mut packs: Vec<Pack> = Vec::new();
    for pack in &args.packs {
        if !pack.exists() {
            return Err(format!("Input does not exist: {}", pack.display()));
        }
        if !pack.is_file() {
            return Err(format!("Input is not a file: {}", pack.display()));
        }

        let temp = match scan_pack(&pack) {
            Err(error) => {
                return Err(format!("{}: {}", pack.display(), error.to_string()));
            }
            Ok(pack) => pack,
        };
        packs.push(temp);
    }

    if args.dest.exists() {
        if !args.dest.is_dir() {
            return Err(format!(
                "Output path exists and is not a directory: {}",
                args.dest.display()
            ));
        }
    } else {
        debug!("Creating output directory: {}", &args.dest.display());
        match std::fs::create_dir_all(&args.dest) {
            Err(err) => {
                return Err(format!(
                    "Failed to create {}: {}",
                    &args.dest.display(),
                    err.to_string()
                ));
            }
            _ => {}
        }
    }

    for pack in &packs {
        info!("Extracting files from {}", pack.name);
        let input = match std::fs::File::open(Path::new(&pack.path)) {
            Ok(file) => file,
            Err(error) => {
                return Err(format!(
                    "Failed to open {}: {}",
                    pack.path.display(),
                    error.to_string()
                ));
            }
        };

        let mut reader = BufReader::new(input);

        for (file_index, file) in pack.files.iter().enumerate() {
            let seek_amount = file.data_offset as i64 - reader.stream_position().unwrap() as i64;
            reader.seek_relative(seek_amount).unwrap();

            let matches_filter = args.filter.is_none()
                || glob_match::glob_match(
                    &args.filter.clone().unwrap(),
                    &file.path.to_str().unwrap(),
                );
            if !matches_filter {
                continue;
            }

            let output_file = &args.dest.join(file.path.to_str().unwrap());
            let output_dir = output_file.parent().unwrap();

            info!(
                "{}: {}/{} => {}",
                pack.name,
                file_index + 1,
                pack.files.len(),
                &file.path.to_str().unwrap()
            );

            if !output_dir.exists() {
                match std::fs::create_dir_all(output_dir) {
                    Ok(_) => {}
                    Err(error) => {
                        return Err(format!(
                            "Failed to create directory {}: {}",
                            output_dir.display(),
                            error.to_string()
                        ));
                    }
                }
            }

            let output = match std::fs::File::create(output_file) {
                Ok(file) => file,
                Err(error) => {
                    return Err(format!(
                        "Failed to open {} for writing: {}",
                        output_file.display(),
                        error.to_string()
                    ));
                }
            };

            let mut writer = BufWriter::with_capacity(file.size_on_disk as usize, output);
            let mut bytes_written = 0u32;

            for chunk in &file.chunks {
                let chunk_index = chunk.index;
                let chunk_size = chunk.size;
                let mut chunk_data = vec![0u8; chunk_size as usize];
                reader.read_exact(&mut chunk_data).unwrap();

                let chunk_is_uncompressed = (chunk_size == LZO_BUFFER_SIZE)
                    || (bytes_written + chunk_size == file.size_on_disk);

                if chunk_is_uncompressed {
                    writer.write_all(&chunk_data).unwrap();
                    bytes_written += chunk_size;
                } else {
                    let decompressed_data = match lzokay_native::decompress_all(
                        &chunk_data,
                        Some(LZO_BUFFER_SIZE as usize),
                    ) {
                        Ok(data) => data,
                        Err(error) => {
                            return Err(format!(
                                "{}: {}: Failed to decompress chunk #{}: {}",
                                pack.name,
                                &file.path.to_str().unwrap(),
                                chunk_index,
                                error.to_string()
                            ));
                        }
                    };
                    writer.write_all(&decompressed_data).unwrap();
                    bytes_written += decompressed_data.len() as u32;
                }
            }
        }
    }

    info!(
        "==> Done! ({:.3?}s)",
        execution_timer.elapsed().as_secs_f32()
    );

    Ok(())
}

fn cmd_list(args: &ListArgs) -> Result<(), String> {
    for pack in &args.packs {
        let pack = scan_pack(&pack)?;
        for (i, file) in pack.files.iter().enumerate() {
            println!(
                "{}: {}/{} ==> {}",
                pack.path.file_name().unwrap().to_str().unwrap(),
                i + 1,
                pack.files.len(),
                file.path.display()
            );
        }
    }

    Ok(())
}

// ---------------------------------------------------------

#[derive(Debug)]
struct Pack {
    path: PathBuf,
    name: String,
    files: Vec<File>,
}

#[derive(Debug)]
struct File {
    index: u32,
    path: PathBuf,
    data_offset: u32,
    size_on_disk: u32,
    size_in_pack: u32,
    chunks: Vec<Chunk>,
}

#[derive(Debug)]
struct Chunk {
    index: u32,
    offset: u32,
    size: u32,
}

fn scan_pack(path: &PathBuf) -> Result<Pack, String> {
    let input = std::fs::File::open(path).unwrap();
    let mut reader = BufReader::new(input);

    let magic: u32 = reader.read_le().unwrap();
    const PACK_MAGIC: u32 = 0x4b434150;
    if magic != PACK_MAGIC {
        return Err("Invalid file signature".to_string());
    }

    let version: u32 = reader.read_le().unwrap();
    const PACK_VERSION: u32 = 0x00030000;
    if version != PACK_VERSION {
        return Err("Unsupported file version".to_string());
    }

    let num_files: u32 = reader.read_le().unwrap();
    let file_section_size: u32 = reader.read_le().unwrap();
    let num_chunks: u32 = reader.read_le().unwrap();

    let mut file_offsets: Vec<u32> = Vec::new();
    for _ in 0..num_files {
        file_offsets.push(reader.read_le().unwrap());
    }

    let mut chunk_sizes: Vec<u32> = Vec::new();
    for _ in 0..num_chunks {
        chunk_sizes.push(reader.read_le().unwrap());
    }

    let mut offset: u32 = reader.stream_position().unwrap() as u32 + file_section_size;
    let mut chunk_offsets: Vec<u32> = Vec::new();
    for i in 0..num_chunks {
        chunk_offsets.push(offset);
        offset += chunk_sizes[i as usize];
    }

    let mut pack = Pack {
        path: path.clone(),
        name: String::from(path.file_name().unwrap().to_str().unwrap()),
        files: Vec::new(),
    };

    for i in 0..num_files {
        let data_offset: u32 = reader.read_le().unwrap();
        let first_chunk: u32 = reader.read_le().unwrap();
        let size_on_disk: u32 = reader.read_le().unwrap();
        let size_in_pack: u32 = reader.read_le().unwrap();
        let path: NullString = reader.read_le().unwrap();

        let mut file = File {
            index: i,
            path: PathBuf::from(path.to_string()),
            data_offset,
            size_on_disk,
            size_in_pack,
            chunks: Vec::new(),
        };

        let mut chunk_index = first_chunk;
        let mut accumulated_size = 0u32;
        while accumulated_size < size_in_pack {
            let chunk_offset = chunk_offsets[chunk_index as usize];
            let chunk_size = chunk_sizes[chunk_index as usize];
            file.chunks.push(Chunk {
                index: chunk_index,
                offset: chunk_offset,
                size: chunk_size,
            });
            accumulated_size += chunk_size;
            chunk_index += 1;
        }

        pack.files.push(file);

        let stream_pos = reader.stream_position().unwrap();
        if stream_pos % 4 != 0 {
            let padding_size = 4 - (stream_pos % 4);
            reader.seek_relative(padding_size as i64).unwrap();
        }
    }

    Ok(pack)
}
