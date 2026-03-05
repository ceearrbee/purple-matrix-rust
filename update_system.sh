sed -i 's/encrypted: false,/encrypted: false,\n            is_system: true,/g' src/ffi/mod.rs
sed -i 's/encrypted: false,/encrypted: false,\n            is_system: false,/g' src/sync_logic.rs
sed -i 's/encrypted: false,/encrypted: false,\n            is_system: true,/g' src/handlers/room_state.rs
sed -i 's/encrypted: false,/encrypted: false,\n            is_system: true,/g' src/handlers/messages.rs
sed -i 's/encrypted: is_encrypted,/encrypted: is_encrypted,\n            is_system: false,/g' src/handlers/messages.rs
sed -i 's/encrypted: is_encrypted,/encrypted: is_encrypted,\n            is_system: false,/g' src/handlers/reactions.rs
sed -i 's/encrypted: false,/encrypted: false,\n            is_system: false,/g' src/handlers/polls.rs
