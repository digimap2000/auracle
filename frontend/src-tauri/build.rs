fn main() -> Result<(), Box<dyn std::error::Error>> {
    tauri_build::build();

    let proto_root = "../../proto";

    tonic_build::configure()
        .build_server(false)
        .compile_protos(
            &[
                &format!("{proto_root}/auracle/inventory/v1/inventory.proto"),
                &format!("{proto_root}/auracle/observation/v1/observation.proto"),
            ],
            &[proto_root],
        )?;

    Ok(())
}
