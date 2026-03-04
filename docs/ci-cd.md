# CI/CD

## GitHub Actions Workflow

**File**: `.github/workflows/release.yml`

### Trigger

- **Tag push**: Any tag matching `v*` (e.g. `v0.1.0`, `v1.0.0-beta.1`)
- **Manual dispatch**: Can be triggered manually from the GitHub Actions UI

### Concurrency

Group: `release-{ref}`. In-progress runs for the same ref are cancelled.

### Build Matrix

| Platform | Runner | Target | Architecture |
|----------|--------|--------|-------------|
| macOS | `macos-latest` | `aarch64-apple-darwin` | Apple Silicon (arm64) |
| macOS | `macos-latest` | `x86_64-apple-darwin` | Intel (x86_64) |

`fail-fast: false` — both builds continue independently even if one fails.

### Build Steps

1. **Checkout** — `actions/checkout@v4`
2. **Setup Node** — `actions/setup-node@v4` with `node-version: lts/*`
3. **Install Rust** — `dtolnay/rust-toolchain@stable` with both macOS targets
4. **Rust cache** — `swatinem/rust-cache@v2`, workspace `./src-tauri -> target`
5. **Install frontend deps** — `npm ci`
6. **Build and release** — `tauri-apps/tauri-action@v0`

### Release Configuration

| Setting | Value |
|---------|-------|
| Tag name | `v__VERSION__` (substituted from `tauri.conf.json` version) |
| Release name | `Auracle v__VERSION__` |
| Release body | "See assets below to download and install." |
| Draft | `true` (must be manually published) |
| Prerelease | `false` |

### Required Secrets

| Secret | Purpose |
|--------|---------|
| `GITHUB_TOKEN` | Automatic — used to create releases and upload assets |
| `TAURI_SIGNING_PRIVATE_KEY` | Ed25519 private key for signing updater artifacts |
| `TAURI_SIGNING_PRIVATE_KEY_PASSWORD` | Password for the signing key (can be empty string) |

### Release Artifacts

The `tauri-apps/tauri-action` produces per-target:
- `.dmg` — macOS disk image installer
- `.app.tar.gz` — Compressed app bundle
- `.app.tar.gz.sig` — Ed25519 signature for the updater
- `latest.json` — Updater manifest (version, download URLs, signatures)

## Auto-Updater

### Configuration

**File**: `src-tauri/tauri.conf.json` → `plugins.updater`

```json
{
  "pubkey": "<base64-encoded ed25519 public key>",
  "endpoints": [
    "https://github.com/digimap2000/auracle/releases/latest/download/latest.json"
  ]
}
```

The `createUpdaterArtifacts: true` setting in `bundle` ensures the `.sig` files and `latest.json` are generated during build.

### Signing Keypair

Generated with `tauri signer generate`. Stored at:
- Private key: `~/.tauri/auracle.key`
- Public key: `~/.tauri/auracle.key.pub`

The private key must be set as `TAURI_SIGNING_PRIVATE_KEY` in GitHub repository secrets.

### Update Flow

1. App checks `latest.json` endpoint on startup (via `useUpdater` hook)
2. If a newer version exists, user is prompted in the StatusBar
3. User clicks download → app downloads `.app.tar.gz` with progress
4. User clicks restart → app relaunches with new version via `@tauri-apps/plugin-process`

### Required Tauri Capabilities

In `src-tauri/capabilities/default.json`:
- `updater:default` — allows checking for and downloading updates
- `process:allow-restart` — allows app relaunch after update

## Release Process

### Creating a Release

1. Update version in `src-tauri/tauri.conf.json` and `package.json`
2. Commit and push
3. Create and push a tag:
   ```bash
   git tag v0.2.0
   git push origin v0.2.0
   ```
4. GitHub Actions builds both architectures and creates a draft release
5. Review the draft release on GitHub and publish it

### Installing Without Code Signing

The macOS builds are unsigned (no Apple Developer ID certificate). Users must bypass Gatekeeper:

**DMG install**:
1. Download the `.dmg` from the GitHub release
2. Open the DMG and drag Auracle to Applications
3. On first launch, macOS will block it
4. Go to System Settings → Privacy & Security → scroll to "Auracle was blocked" → click "Open Anyway"

**Or via terminal**:
```bash
xattr -cr /Applications/Auracle.app
```
