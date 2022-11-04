mod connection;

use connection::{Command, Connection, Response};
use galevm::{ModuleLoader, VM};
use log::info;
use std::{
    net::TcpListener,
    thread,
    sync::mpsc::{channel, Sender, Receiver, TryRecvError}
};

struct Supervisor {
    vms: Vec<VM>,
    names: Vec<String>,
}

impl Supervisor {
    fn start(&mut self) -> Option<String> {
        let mut connections: Vec<(Sender<Response>, Receiver<Command>)> = vec![];
        let listener = match TcpListener::bind("127.0.0.1:1680") {
            Ok(listen) => listen,
            Err(e) => { return Some(format!("{}", e)); },
        };
        listener.set_nonblocking(true).expect("Could not listen nonblocking");

        info!(target: "connection_events", "listening...");

        loop {
            /*
            * FIXME Do this in a blocking manner without sleep such as crossbeam-channels select
            */

            /*
            * Handle new TCP connections
            */
            log::trace!(target: "connection_events", "checking for connection");
            match listener.accept() {
                Ok((connection, _)) => {
                    log::trace!(target: "connection_events","new connection");
                    let (my_send, their_recv) = channel();
                    let (their_send, my_recv) = channel();

                    connections.push((my_send, my_recv));

                    thread::spawn(move || {
                        Connection::new(their_send, their_recv, connection).start();
                    });
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                }
                Err(e) => panic!("encountered IO error: {e}"),
            }

            /*
            * Handle commands from connections
            */
            log::trace!(target: "connection_events","checking for commands");
            let mut closed_connections: Vec<usize> = vec![];
            for (i, (send, recv)) in connections.iter().enumerate() {
                match recv.try_recv() {
                    Ok(Command::CloseConnection) => {
                        closed_connections.push(i);
                    }
                    Ok(command) => {
                        log::info!(target: "connection_events",  "received command");
                        match send.send(self.process_command(command)) {
                            Ok(_) => {
                                log::info!(target: "connection_events", "processed command");
                            },
                            Err(e) => { return Some(format!("{}", e)); },
                        }
                    },
                    Err(TryRecvError::Disconnected) => {
                        closed_connections.push(i);
                    }
                    Err(TryRecvError::Empty) => {},
                }
            }

            // Reverse so that we are not shifting connections that we process in a later iteration
            for closed in closed_connections.iter().rev() {
                connections.remove(*closed);
            }

            std::thread::sleep(std::time::Duration::from_millis(100));
        }
    }

    fn process_command(&mut self, command: Command) -> Response {
        match command {
            Command::CreateVM(name_opt) => {
                let name = match name_opt {
                    Some(n) => String::from(n),
                    None => format!("vm_{}", self.vms.len()),
                };
                let id = self.vms.len();

                let message = format!("created vm {{ name: {:?}, id: {} }}\n", name, id);
                info!(target: "connection_events", "{}", message);

                let ml = ModuleLoader::new();
                let vm = VM::new(ml);
                self.vms.push(vm);
                self.names.push(name.clone());
                Response::VMIdentifier(name, id) 
            }
            Command::ListVMs => {
                let mut message = vec![];

                for (i, name) in self.names.iter().enumerate() {
                    message.push((name.clone(), i))
                }

                Response::ListOfVMs(message)
            },
            Command::SummarizeVM(id) => {
                Response::SummaryOfVM { name: self.names.get(id).unwrap().clone(), id }
            },
            Command::CloseConnection => panic!("Unexpected command"),
        }
    }


}


fn main() -> std::io::Result<()> {
    simple_logger::SimpleLogger::new().env().init().unwrap();

    let mut supervisor = Supervisor{ vms: vec![], names: vec![] };
    supervisor.start();
    Ok(())
}
