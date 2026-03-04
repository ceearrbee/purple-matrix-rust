import re

mapping = {
    100: 1,   # MessageReceived
    101: 2,   # Typing
    102: 3,   # RoomJoined
    103: 4,   # RoomLeft
    104: 5,   # ReadMarker
    105: 6,   # Presence
    106: 7,   # ChatTopic
    107: 8,   # ChatUser
    108: 9,   # Invite
    109: 12,  # LoginFailed
    110: 25,  # Connected
    111: 24,  # SsoUrl
    112: 26,  # SasRequest
    113: 27,  # SasHaveEmoji
    114: 28,  # ShowVerificationQr
    115: 15,  # PollList
    116: 10,  # RoomListAdd
    117: 11,  # RoomPreview
    118: 14,  # ThreadList
    119: 13,  # ShowUserInfo
    120: 21,  # StickerPack
    121: 23,  # StickerDone
    122: 22,  # Sticker
}

with open("src/ffi/mod.rs", "r") as f:
    content = f.read()

for old, new in mapping.items():
    # Replace in poll_event: `=> (\n                100,`
    content = re.sub(r'=> \(\n\s+' + str(old) + r',', '=> (\n                ' + str(new) + ',', content)
    # Replace in free_event: `\n            100 => {`
    content = re.sub(r'\n\s+' + str(old) + r' => \{', '\n            ' + str(new) + ' => {', content)

with open("src/ffi/mod.rs", "w") as f:
    f.write(content)
print("Updated src/ffi/mod.rs events mapping.")
