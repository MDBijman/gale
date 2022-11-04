use std::{sync::mpsc::{Sender, Receiver}, net::{TcpStream, Shutdown}, io::{BufReader, Write, BufRead}};

use log::info;

pub enum Command {
    CreateVM(Option<String>),
    ListVMs,
    CloseConnection,
    SummarizeVM(usize)
}

pub enum Response {
    VMIdentifier(String, usize),
    ListOfVMs(Vec<(String, usize)>),
    SummaryOfVM {
        name: String,
        id: usize,

    }
}

pub struct Connection {
    send: Sender<Command>,
    recv: Receiver<Response>,
    tcp_con: TcpStream
}

impl Connection {
    pub fn new(send: Sender<Command>, recv: Receiver<Response>, tcp_con: TcpStream) -> Connection {
        Connection {
            send, recv, tcp_con
        }
    }

    pub fn start(&mut self) {
        info!(target: "connection_events", "new connection {}", self.tcp_con.peer_addr().map(|a| a.to_string()).unwrap_or(String::from("?")));
        self.tcp_con.set_nonblocking(false).unwrap();

        let second_connection = match self.tcp_con.try_clone() {
            Ok(c) => c,
            Err(_) => {
                self.tcp_con.shutdown(Shutdown::Both).unwrap();
                return;
            }
        };

        let mut reader = BufReader::new(second_connection);

        loop {
            let mut buffer = String::new();
            let len = reader.read_line(&mut buffer).expect("Error reading line");

            if len == 0 {
                break;
            }

            assert!(buffer.ends_with("\n"));
            buffer.pop();

            info!(target: "connection_events", "got message: {:?}", buffer);

            if buffer == "help" {
                let message = "Commands: help, create name?, list";
                self.tcp_con.write_all(message.as_bytes()).expect("failure sending message");
                self.tcp_con.flush().unwrap();
            } else if buffer.starts_with("create") {
                let mut parts = buffer.split(" ");
                parts.next();
                self.send.send(Command::CreateVM(parts.next().map(|s| String::from(s)))).unwrap();
                match self.recv.recv() {
                    Ok(Response::VMIdentifier(name, id)) => {
                        let message = format!("created vm {{ name: {:?}, id: {} }}\n", name, id);
                        self.tcp_con.write_all(message.as_bytes()).expect("failure sending message");
                        self.tcp_con.flush().unwrap();
                    }
                    Ok(_) => panic!("Unexpected response"),
                    Err(_) => panic!("Failure in receiving response")
                }
            } else if buffer.starts_with("summary") {
                let mut parts = buffer.split(" ");
                parts.next();
                let id = parts.next().unwrap().parse::<usize>().unwrap();
                self.send.send(Command::SummarizeVM(id)).unwrap();
                match self.recv.recv() {
                    Ok(Response::SummaryOfVM { name, id }) => {
                        let message = format!("({}, {})\n", name, id);
                        self.tcp_con.write_all(message.as_bytes()).expect("failure sending message");
                        self.tcp_con.flush().unwrap();
                    }
                    Ok(_) => panic!("Unexpected response"),
                    Err(_) => panic!("Failure in receiving response")
                }
            } else if buffer == "list" {
                info!(target: "connection_events", "listing vms");
                self.send.send(Command::ListVMs).unwrap();
                match self.recv.recv() {
                    Ok(Response::ListOfVMs(elems)) => {
                        let mut message = format!("[");

                        for (i, (name, id)) in elems.iter().enumerate() {
                            if i < elems.len() - 1 {
                                message.push_str(format!("({}, {}), ", name, id).as_str());
                            } else {
                                message.push_str(format!("({}, {})", name, id).as_str());
                            }
                        }

                        message.push(']');
                        message.push('\n');
                        self.tcp_con.write_all(message.as_bytes()).expect("failure sending message");
                        self.tcp_con.flush().unwrap();
                    }
                    Ok(_) => panic!("Unexpected response"),
                    Err(_) => panic!("Failure in receiving response")
                }
            } else if buffer == "exit" {
                self.send.send(Command::CloseConnection).unwrap();
                self.tcp_con.shutdown(Shutdown::Both).unwrap();
                info!(target: "connection_events", "shutting down connection {}", self.tcp_con.peer_addr().map(|a| a.to_string()).unwrap_or(String::from("?")));
                break;
            } else {
                self.tcp_con.write_all("Unknown command\n".as_bytes()).unwrap();
                self.tcp_con.flush().unwrap();
                info!(target: "connection_events", "Unknown command {}", buffer);
            }
        }

    }
}