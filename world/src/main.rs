// ---------------------------------------------------------
// Configuration
// ---------------------------------------------------------

struct Config {
    data_dir: std::path::PathBuf,
}

impl Config {
    fn new() -> Self {
        Config {
            data_dir: std::path::PathBuf::from("../../../../tools/unpacker/data"),
        }
    }
}

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------

fn load_text_file(filepath: &std::path::PathBuf) -> Vec<String> {
    let mut result = Vec::new();
    for line in std::fs::read_to_string(filepath).unwrap().lines() {
        result.push(line.to_string())
    }

    result
}

// ---------------------------------------------------------
// Culture
// ---------------------------------------------------------

#[derive(Debug)]
struct Culture {
    id: u32,
    name: String,
}

impl Culture {
    fn load_all(file: &std::path::PathBuf) -> Vec<Culture> {
        let mut cultures: Vec<Culture> = Vec::new();

        let lines = load_text_file(&file);
        for line in lines {
            if !line.starts_with("culture") {
                continue;
            }
            let mut parts = line.split_whitespace();
            let name = parts.nth(1).unwrap().to_string();
            let id = cultures.len() as u32;
            cultures.push(Culture { id, name });
        }

        cultures
    }
}

// ---------------------------------------------------------
// Religion
// ---------------------------------------------------------

#[derive(Debug)]
struct Religion {
    id: u32,
    name: String,
}

impl Religion {
    fn load_all(file: &std::path::PathBuf) -> Vec<Religion> {
        let mut religions: Vec<Religion> = Vec::new();

        let mut lines = load_text_file(&file);
        while !lines.first().unwrap().starts_with("religions") {
            lines.remove(0);
        }
        lines.remove(0);
        lines.remove(0);
        while !lines.first().unwrap().starts_with("}") {
            let name = lines.first().unwrap().trim().to_string();
            let id = religions.len() as u32;
            religions.push(Religion { id, name });
            lines.remove(0);
        }

        religions
    }
}

// ---------------------------------------------------------
// Faction
// ---------------------------------------------------------

#[derive(Debug)]
struct Faction {
    id: u32,
    name: String,
}

impl Faction {
    fn load_all(file: &std::path::PathBuf) -> Vec<Faction> {
        let mut factions: Vec<Faction> = Vec::new();

        let lines = load_text_file(&file);
        for line in lines {
            if !line.starts_with("faction") {
                continue;
            }
            let mut parts = line.split_whitespace();
            let name = parts.nth(1).unwrap().to_string();
            let id = factions.len() as u32;
            factions.push(Faction { id, name });
        }

        factions
    }
}

// ---------------------------------------------------------
// World
// ---------------------------------------------------------

#[derive(Debug)]
struct World {
    cultures: Vec<Culture>,
    religions: Vec<Religion>,
    factions: Vec<Faction>,
}

impl World {
    fn load() -> Self {
        let config = Config::new();

        World {
            cultures: Culture::load_all(&config.data_dir.join("descr_cultures.txt")),
            religions: Religion::load_all(&config.data_dir.join("descr_religions.txt")),
            factions: Faction::load_all(&config.data_dir.join("descr_sm_factions.txt")),
        }
    }
}

// ---------------------------------------------------------

fn main() {
    let world = World::load();
    println!("{:#?}", world);
}
