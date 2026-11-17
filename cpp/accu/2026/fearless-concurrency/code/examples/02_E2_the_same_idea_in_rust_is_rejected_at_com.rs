use std::thread;

fn main() {
    let mut progress = 0;

    // error[E0373]: closure may outlive the current function, but it
    // borrows `progress`, which is owned by the current function
    let reporter = thread::spawn(|| {
        println!("progress: {}", progress);
    });

    for _ in 0..100 {
        progress += 1;
    }
    reporter.join().unwrap();
}
